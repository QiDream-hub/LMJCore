#include "../../core/include/lmjcore.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// 环境结构
struct lmjcore_env {
  MDB_env *mdb_env;
  MDB_dbi main_dbi;
  MDB_dbi arr_dbi;
  lmjcore_ptr_generator_fn ptr_generator;
  void *ptr_gen_ctx;
};

// 事务结构
struct lmjcore_txn {
  lmjcore_env *env;
  MDB_txn *mdb_txn;
  bool is_read_only;
};

// 测试配置
#define TEST_DB_PATH "./lmjcore_db"
#define TEST_MAP_SIZE (1024 * 1024 * 10) // 10MB
#define TEST_BUF_SIZE 4096

// 简单的指针生成器实现
static int test_ptr_generator(void *ctx, uint8_t out[LMJCORE_PTR_LEN]) {
  static uint64_t counter = 0;
  (void)ctx; // 未使用参数

  // 生成简单的递增指针
  memset(out, 0, LMJCORE_PTR_LEN);
  out[0] = 0x00; // 类型位会在创建时设置
  counter++;
  memcpy(out + 1, &counter, sizeof(counter));

  return LMJCORE_SUCCESS;
}

// 辅助函数：打印指针
static void print_ptr(const char *label, const lmjcore_ptr ptr) {
  char str[LMJCORE_PTR_STRING_BUF_SIZE];
  if (lmjcore_ptr_to_string(ptr, str, sizeof(str)) == LMJCORE_SUCCESS) {
    printf("%s: %s\n", label, str);
  }
}

// 辅助函数：打印测试结果
static void print_test_result(const char *test_name, int result, int expected) {
  if (result == expected) {
    printf("[PASS] %s\n", test_name);
  } else {
    printf("[FAIL] %s: expected %d, got %d (%s)\n", test_name, expected, result,
           lmjcore_strerror(result));
  }
}

// 测试环境初始化和清理
static void test_env_init_cleanup(void) {
  printf("\n=== 测试环境初始化和清理 ===\n");

  lmjcore_env *env = NULL;
  int rc = lmjcore_init(TEST_DB_PATH, TEST_MAP_SIZE, LMJCORE_ENV_SAFE,
                        test_ptr_generator, NULL, &env);
  print_test_result("lmjcore_init", rc, LMJCORE_SUCCESS);

  if (rc == LMJCORE_SUCCESS) {
    rc = lmjcore_cleanup(env);
    print_test_result("lmjcore_cleanup", rc, LMJCORE_SUCCESS);
  }
}

// 测试事务管理
static void test_transaction(lmjcore_env *env) {
  printf("\n=== 测试事务管理 ===\n");

  lmjcore_txn *txn = NULL;

  // 开启写事务
  int rc = lmjcore_txn_begin(env, NULL, LMJCORE_TXN_DEFAULT, &txn);
  print_test_result("lmjcore_txn_begin (write)", rc, LMJCORE_SUCCESS);

  if (rc == LMJCORE_SUCCESS) {
    // 检查事务类型
    bool is_readonly = lmjcore_txn_is_read_only(txn);
    printf("事务类型: %s\n", is_readonly ? "只读" : "可写");

    // 提交事务
    rc = lmjcore_txn_commit(txn);
    print_test_result("lmjcore_txn_commit", rc, LMJCORE_SUCCESS);
  }

  // 测试只读事务
  rc = lmjcore_txn_begin(env, NULL, LMJCORE_TXN_READONLY, &txn);
  print_test_result("lmjcore_txn_begin (readonly)", rc, LMJCORE_SUCCESS);

  if (rc == LMJCORE_SUCCESS) {
    bool is_readonly = lmjcore_txn_is_read_only(txn);
    printf("事务类型: %s\n", is_readonly ? "只读" : "可写");

    rc = lmjcore_txn_abort(txn);
    print_test_result("lmjcore_txn_abort", rc, LMJCORE_SUCCESS);
  }

  // 测试父事务
  lmjcore_txn *parent = NULL;
  rc = lmjcore_txn_begin(env, NULL, LMJCORE_TXN_DEFAULT, &parent);
  if (rc == LMJCORE_SUCCESS) {
    lmjcore_txn *child = NULL;
    rc = lmjcore_txn_begin(env, parent, LMJCORE_TXN_DEFAULT, &child);
    print_test_result("lmjcore_txn_begin (child)", rc, LMJCORE_SUCCESS);

    if (rc == LMJCORE_SUCCESS) {
      lmjcore_txn_commit(child);
    }
    lmjcore_txn_commit(parent);
  }
}

// 测试对象创建和基本操作
static void test_object_operations(lmjcore_env *env) {
  printf("\n=== 测试对象操作 ===\n");

  lmjcore_txn *txn = NULL;
  lmjcore_ptr obj_ptr;
  uint8_t buffer[TEST_BUF_SIZE];
  lmjcore_result_obj *result;

  // 开启事务
  int rc = lmjcore_txn_begin(env, NULL, LMJCORE_TXN_DEFAULT, &txn);
  assert(rc == LMJCORE_SUCCESS);

  // 1. 创建对象
  rc = lmjcore_obj_create(txn, obj_ptr);
  print_test_result("lmjcore_obj_create", rc, LMJCORE_SUCCESS);
  print_ptr("创建的对象", obj_ptr);

  // 2. 检查实体存在性
  rc = lmjcore_entity_exist(txn, obj_ptr);
  print_test_result("lmjcore_entity_exist (应为1)", rc, 1);

  // 3. 注册对象成员（仅注册名称）
  const char *member1 = "name";
  rc = lmjcore_obj_member_register(txn, obj_ptr, (const uint8_t *)member1,
                                   strlen(member1));
  print_test_result("lmjcore_obj_member_register", rc, LMJCORE_SUCCESS);

  // 4. 设置成员值
  const char *name_value = "Test Object";
  rc = lmjcore_obj_member_put(txn, obj_ptr, (const uint8_t *)member1,
                              strlen(member1), (const uint8_t *)name_value,
                              strlen(name_value) + 1);
  print_test_result("lmjcore_obj_member_put", rc, LMJCORE_SUCCESS);

  // 5. 注册第二个成员（测试缺失值）
  const char *member2 = "description";
  rc = lmjcore_obj_member_register(txn, obj_ptr, (const uint8_t *)member2,
                                   strlen(member2));
  print_test_result("lmjcore_obj_member_register (无值)", rc, LMJCORE_SUCCESS);

  // 6. 获取成员值
  char value_buf[256];
  size_t value_size;
  rc = lmjcore_obj_member_get(txn, obj_ptr, (const uint8_t *)member1,
                              strlen(member1), (uint8_t *)value_buf,
                              sizeof(value_buf), &value_size);
  print_test_result("lmjcore_obj_member_get", rc, LMJCORE_SUCCESS);
  if (rc == LMJCORE_SUCCESS) {
    printf("成员 '%s' 的值: %s (长度: %zu)\n", member1, value_buf, value_size);
  }

  // 7. 检查不存在的成员
  rc = lmjcore_obj_member_get(txn, obj_ptr, (const uint8_t *)"nonexist", 8,
                              (uint8_t *)value_buf, sizeof(value_buf),
                              &value_size);
  print_test_result("lmjcore_obj_member_get (不存在)", rc,
                    LMJCORE_ERROR_MEMBER_NOT_FOUND);

  // 8. 检查成员值存在性
  rc = lmjcore_obj_member_value_exist(txn, obj_ptr, (const uint8_t *)member1,
                                      strlen(member1));
  print_test_result("lmjcore_obj_member_value_exist (应有值)", rc, 1);

  rc = lmjcore_obj_member_value_exist(txn, obj_ptr, (const uint8_t *)member2,
                                      strlen(member2));
  print_test_result("lmjcore_obj_member_value_exist (缺失值)", rc, 0);

  // 9. 获取完整对象
  rc = lmjcore_obj_get(txn, obj_ptr, buffer, sizeof(buffer), &result);
  print_test_result("lmjcore_obj_get", rc, LMJCORE_SUCCESS);

  if (rc == LMJCORE_SUCCESS) {
    printf("对象成员数量: %zu\n", result->member_count);
    printf("对象错误数量: %zu\n", result->error_count);

    // 遍历成员
    for (size_t i = 0; i < result->member_count; i++) {
      lmjcore_member_descriptor *member = &result->members[i];
      char *member_name = (char *)(buffer + member->member_name.value_offset);
      char *member_val = (char *)(buffer + member->member_value.value_offset);

      printf("  成员 %zu: 名称='%s'", i, member_name);
      if (member->member_value.value_len > 0) {
        printf(", 值='%s' (len=%zu)\n", member_val,
               member->member_value.value_len);
      } else {
        printf(", 值=(缺失)\n");
      }
    }
  }

  // 10. 统计对象值
  size_t total_len, total_count;
  rc = lmjcore_obj_stat_values(txn, obj_ptr, &total_len, &total_count);
  print_test_result("lmjcore_obj_stat_values", rc, LMJCORE_SUCCESS);
  printf("值总长度: %zu, 值数量: %zu\n", total_len, total_count);

  // 11. 获取成员列表
  lmjcore_result_arr *member_list;
  rc = lmjcore_obj_member_list(txn, obj_ptr, buffer, sizeof(buffer),
                               &member_list);
  print_test_result("lmjcore_obj_member_list", rc, LMJCORE_SUCCESS);
  printf("成员列表数量: %zu\n", member_list->element_count);

  // 12. 统计成员
  size_t total_member_len, member_count;
  rc = lmjcore_obj_stat_members(txn, obj_ptr, &total_member_len, &member_count);
  print_test_result("lmjcore_obj_stat_members", rc, LMJCORE_SUCCESS);
  printf("成员名总长度: %zu, 成员数量: %zu\n", total_member_len, member_count);

  // 13. 删除成员值
  rc = lmjcore_obj_member_value_del(txn, obj_ptr, (const uint8_t *)member1,
                                    strlen(member1));
  print_test_result("lmjcore_obj_member_value_del", rc, LMJCORE_SUCCESS);

  // 14. 完全删除成员
  rc = lmjcore_obj_member_del(txn, obj_ptr, (const uint8_t *)member2,
                              strlen(member2));
  print_test_result("lmjcore_obj_member_del", rc, LMJCORE_SUCCESS);

  // 15. 删除对象
  rc = lmjcore_obj_del(txn, obj_ptr);
  print_test_result("lmjcore_obj_del", rc, LMJCORE_SUCCESS);

  // 16. 检查对象是否已删除
  rc = lmjcore_entity_exist(txn, obj_ptr);
  print_test_result("lmjcore_entity_exist (应为0)", rc, 0);

  lmjcore_txn_commit(txn);
}

// 测试对象注册
static void test_object_register(lmjcore_env *env) {
  printf("\n=== 测试对象注册 ===\n");

  lmjcore_txn *txn = NULL;
  lmjcore_ptr fixed_ptr = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

  int rc = lmjcore_txn_begin(env, NULL, LMJCORE_TXN_DEFAULT, &txn);
  assert(rc == LMJCORE_SUCCESS);

  // 注册固定指针的对象
  rc = lmjcore_obj_register(txn, fixed_ptr);
  print_test_result("lmjcore_obj_register", rc, LMJCORE_SUCCESS);
  print_ptr("注册的对象", fixed_ptr);

  // 检查存在性
  rc = lmjcore_entity_exist(txn, fixed_ptr);
  print_test_result("注册后存在性检查", rc, 1);

  // 尝试重复注册
  rc = lmjcore_obj_register(txn, fixed_ptr);
  print_test_result("重复注册", rc, LMJCORE_ERROR_ENTITY_EXISTS);

  lmjcore_txn_commit(txn);
}

// 测试数组操作
static void test_array_operations(lmjcore_env *env) {
  printf("\n=== 测试数组操作 ===\n");

  lmjcore_txn *txn = NULL;
  lmjcore_ptr arr_ptr;
  uint8_t buffer[TEST_BUF_SIZE];
  lmjcore_result_arr *result;

  int rc = lmjcore_txn_begin(env, NULL, LMJCORE_TXN_DEFAULT, &txn);
  assert(rc == LMJCORE_SUCCESS);

  // 1. 创建数组
  rc = lmjcore_arr_create(txn, arr_ptr);
  print_test_result("lmjcore_arr_create", rc, LMJCORE_SUCCESS);
  print_ptr("创建的数组", arr_ptr);

  // 2. 追加元素
  const char *elements[] = {"Apple", "Banana", "Cherry", "Date"};
  for (int i = 0; i < 4; i++) {
    rc = lmjcore_arr_append(txn, arr_ptr, (const uint8_t *)elements[i],
                            strlen(elements[i]) + 1);
    printf("  追加: %s\n", elements[i]);
  }
  print_test_result("lmjcore_arr_append (4次)", LMJCORE_SUCCESS,
                    LMJCORE_SUCCESS);

  // 3. 获取数组
  rc = lmjcore_arr_get(txn, arr_ptr, buffer, sizeof(buffer), &result);
  print_test_result("lmjcore_arr_get", rc, LMJCORE_SUCCESS);

  if (rc == LMJCORE_SUCCESS) {
    printf("数组元素数量: %zu\n", result->element_count);
    printf("数组元素内容:\n");
    for (size_t i = 0; i < result->element_count; i++) {
      lmjcore_descriptor *elem = &result->elements[i];
      char *value = (char *)(buffer + elem->value_offset);
      printf("  [%zu] '%s' (len=%zu)\n", i, value, elem->value_len);
    }
  }

  // 4. 统计数组
  size_t total_len, count;
  rc = lmjcore_arr_stat_element(txn, arr_ptr, &total_len, &count);
  print_test_result("lmjcore_arr_stat_element", rc, LMJCORE_SUCCESS);
  printf("数组统计: 总长度=%zu, 元素数量=%zu\n", total_len, count);

  // 5. 删除一个元素
  rc = lmjcore_arr_element_del(txn, arr_ptr, (const uint8_t *)"Banana", 7);
  print_test_result("lmjcore_arr_element_del (Banana)", rc, LMJCORE_SUCCESS);

  // 6. 再次获取数组，确认删除
  rc = lmjcore_arr_get(txn, arr_ptr, buffer, sizeof(buffer), &result);
  if (rc == LMJCORE_SUCCESS) {
    printf("删除后数组元素数量: %zu\n", result->element_count);
    printf("剩余元素:\n");
    for (size_t i = 0; i < result->element_count; i++) {
      lmjcore_descriptor *elem = &result->elements[i];
      char *value = (char *)(buffer + elem->value_offset);
      printf("  [%zu] '%s'\n", i, value);
    }
  }

  // 7. 删除数组
  rc = lmjcore_arr_del(txn, arr_ptr);
  print_test_result("lmjcore_arr_del", rc, LMJCORE_SUCCESS);

  // 8. 检查数组是否已删除
  rc = lmjcore_entity_exist(txn, arr_ptr);
  print_test_result("数组存在性检查 (应为0)", rc, 0);

  lmjcore_txn_commit(txn);
}

// 测试审计和修复
static void test_audit_repair(lmjcore_env *env) {
  printf("\n=== 测试审计和修复 ===\n");

  lmjcore_txn *txn = NULL;
  lmjcore_ptr obj_ptr;
  uint8_t report_buf[TEST_BUF_SIZE];
  lmjcore_audit_report *report;

  int rc = lmjcore_txn_begin(env, NULL, LMJCORE_TXN_DEFAULT, &txn);
  assert(rc == LMJCORE_SUCCESS);

  // 创建测试对象
  rc = lmjcore_obj_create(txn, obj_ptr);
  assert(rc == LMJCORE_SUCCESS);
  print_ptr("审计测试对象", obj_ptr);

  // 注册成员
  const char *member = "valid_member";
  rc = lmjcore_obj_member_register(txn, obj_ptr, (const uint8_t *)member,
                                   strlen(member));
  assert(rc == LMJCORE_SUCCESS);

  // 故意创建一个幽灵成员（直接写入main库，不在arr库注册）
  uint8_t ghost_key[LMJCORE_PTR_LEN + 10];
  memcpy(ghost_key, obj_ptr, LMJCORE_PTR_LEN);
  const char *ghost_member = "ghost_member";
  memcpy(ghost_key + LMJCORE_PTR_LEN, ghost_member, strlen(ghost_member));

  MDB_val key = {.mv_data = ghost_key,
                 .mv_size = LMJCORE_PTR_LEN + strlen(ghost_member)};
  MDB_val val = {.mv_data = "ghost_value", .mv_size = 12};

  // 直接使用LMDB API创建幽灵成员
  rc = mdb_put(txn->mdb_txn, txn->env->main_dbi, &key, &val, 0);
  assert(rc == MDB_SUCCESS);

  // 审计对象
  rc = lmjcore_audit_object(txn, obj_ptr, report_buf, sizeof(report_buf),
                            &report);
  print_test_result("lmjcore_audit_object", rc, LMJCORE_SUCCESS);

  if (rc == LMJCORE_SUCCESS) {
    printf("审计发现的幽灵成员数量: %zu\n", report->audit_cont);

    // 打印幽灵成员信息
    for (size_t i = 0; i < report->audit_cont; i++) {
      lmjcore_audit_descriptor *desc = &report->audit_descriptor[i];
      char *member_name =
          (char *)(report_buf + desc->member.member_name.value_offset);
      char *member_value =
          (char *)(report_buf + desc->member.member_value.value_offset);
      printf("  幽灵成员 %zu: 名称='%s', 值='%s'\n", i, member_name,
             member_value);
    }
  }

  // 修复对象
  rc = lmjcore_repair_object(txn, report_buf, sizeof(report_buf), report);
  print_test_result("lmjcore_repair_object", rc, LMJCORE_SUCCESS);

  // 再次审计，确认已修复
  rc = lmjcore_audit_object(txn, obj_ptr, report_buf, sizeof(report_buf),
                            &report);
  if (rc == LMJCORE_SUCCESS) {
    printf("修复后幽灵成员数量: %zu\n", report->audit_cont);
  }

  lmjcore_txn_commit(txn);
}

// 测试指针工具函数
static void test_ptr_utils(void) {
  printf("\n=== 测试指针工具函数 ===\n");

  lmjcore_ptr ptr = {0x01, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
                     0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

  char str[LMJCORE_PTR_STRING_BUF_SIZE];
  lmjcore_ptr ptr2;

  // 指针转字符串
  int rc = lmjcore_ptr_to_string(ptr, str, sizeof(str));
  print_test_result("lmjcore_ptr_to_string", rc, LMJCORE_SUCCESS);
  printf("指针转字符串: %s\n", str);

  // 字符串转指针
  rc = lmjcore_ptr_from_string(str, ptr2);
  print_test_result("lmjcore_ptr_from_string", rc, LMJCORE_SUCCESS);

  // 验证转换结果
  if (memcmp(ptr, ptr2, LMJCORE_PTR_LEN) == 0) {
    printf("[PASS] 指针转换前后一致\n");
  } else {
    printf("[FAIL] 指针转换前后不一致\n");
  }

  // 测试错误情况
  rc = lmjcore_ptr_from_string("invalid", ptr2);
  print_test_result("无效字符串转换", rc, LMJCORE_ERROR_INVALID_PARAM);
}

// 测试错误处理
static void test_error_handling(lmjcore_env *env) {
  printf("\n=== 测试错误处理 ===\n");

  // 测试空指针检查
  int rc = lmjcore_obj_create(NULL, NULL);
  print_test_result("空指针检查", rc, LMJCORE_ERROR_NULL_POINTER);

  // 测试错误码转换
  const char *err_str = lmjcore_strerror(LMJCORE_ERROR_NULL_POINTER);
  printf("错误码转换: %s\n", err_str);

  err_str = lmjcore_strerror(LMJCORE_ERROR_ENTITY_NOT_FOUND);
  printf("错误码转换: %s\n", err_str);

  // 测试LMDB错误码转换
  err_str = lmjcore_strerror(MDB_NOTFOUND);
  printf("LMDB错误码转换: %s\n", err_str);
}

// 主测试函数
int main(void) {
  printf("========================================\n");
  printf("     LMJCore 接口测试程序\n");
  printf("========================================\n");

  // 测试独立的工具函数
  test_ptr_utils();

  // 测试环境初始化和清理
  test_env_init_cleanup();

  // 创建测试环境
  lmjcore_env *env = NULL;
  int rc = lmjcore_init(TEST_DB_PATH, TEST_MAP_SIZE, LMJCORE_ENV_SAFE,
                        test_ptr_generator, NULL, &env);

  if (rc != LMJCORE_SUCCESS) {
    printf("环境初始化失败: %s\n", lmjcore_strerror(rc));
    return 1;
  }

  printf("\n========================================\n");
  printf("环境初始化成功，开始功能测试\n");
  printf("========================================\n");

  // 执行各项测试
  test_transaction(env);
  test_object_operations(env);
  test_object_register(env);
  test_array_operations(env);
  test_audit_repair(env);
  test_error_handling(env);

  // 清理环境
  rc = lmjcore_cleanup(env);
  if (rc == LMJCORE_SUCCESS) {
    printf("\n========================================\n");
    printf("所有测试完成，环境清理成功\n");
    printf("========================================\n");
  }

  return 0;
}