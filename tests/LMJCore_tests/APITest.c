#include "../../core/include/lmjcore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 简单指针生成器
static int test_ptr_generator(void *ctx, uint8_t out[LMJCORE_PTR_LEN]) {
  static uint64_t counter = 1;
  out[0] = 0x01; // 对象类型

  // 填充递增的ID
  for (int i = 0; i < 8; i++) {
    out[1 + i] = (counter >> (56 - i * 8)) & 0xFF;
  }

  // 填充随机数据
  for (int i = 0; i < 8; i++) {
    out[9 + i] = rand() % 256;
  }

  counter++;
  return LMJCORE_SUCCESS;
}

// 测试宏
#define TEST(func_call, desc)                                                  \
  do {                                                                         \
    int ret = func_call;                                                       \
    if (ret != LMJCORE_SUCCESS) {                                              \
      printf("FAILED: %s (error: %d)\n", desc, ret);                           \
      return -1;                                                               \
    } else {                                                                   \
      printf("OK: %s\n", desc);                                                \
    }                                                                          \
  } while (0)

int main() {
  printf("LMJCore 接口简单可用性测试\n");
  printf("===========================\n\n");

  lmjcore_env *env = NULL;
  lmjcore_txn *write_txn = NULL;
  lmjcore_txn *read_txn = NULL;
  lmjcore_ptr obj_ptr, arr_ptr;
  char str_buf[35];

  // 1. 初始化环境 - 使用指定的数据库路径
  printf("1. 测试环境初始化...\n");
  TEST(lmjcore_init("./lmjcore_db/API/_1", 1024 * 1024, 0 | LMJCORE_FLAGS_NOSUBDIR, test_ptr_generator,
                    NULL, &env),
       "lmjcore_init 使用路径: ./lmjcore_db/API");

  // ==================== 测试标准接口流程 ====================

  // 2. 创建写事务 - 用于创建和写入
  printf("\n2. 测试标准对象创建和赋值 (使用写事务)...\n");
  TEST(lmjcore_txn_begin(env, LMJCORE_TXN_WRITE, &write_txn),
       "lmjcore_txn_begin (LMJCORE_TXN_WRITE)");

  // 3. 创建对象
  TEST(lmjcore_obj_create(write_txn, obj_ptr), "lmjcore_obj_create");

  // 4. 使用标准 put 接口为对象添加成员并赋值
  const char *member_name1 = "username";
  const char *member_value1 = "张三";
  TEST(lmjcore_obj_member_put(write_txn, obj_ptr, (const uint8_t *)member_name1,
                              strlen(member_name1),
                              (const uint8_t *)member_value1,
                              strlen(member_value1)),
       "lmjcore_obj_member_put (标准接口: 注册+赋值)");

  // 5. 再添加一个成员
  const char *member_name2 = "age";
  const char *member_value2 = "30";
  TEST(lmjcore_obj_member_put(write_txn, obj_ptr, (const uint8_t *)member_name2,
                              strlen(member_name2),
                              (const uint8_t *)member_value2,
                              strlen(member_value2)),
       "lmjcore_obj_member_put (第二个成员)");

  // 6. 提交写事务
  TEST(lmjcore_txn_commit(write_txn), "提交标准对象创建事务");
  write_txn = NULL; // 事务句柄已失效

  // ==================== 测试高级注册接口 ====================

  printf("\n3. 测试高级成员注册接口 (使用写事务)...\n");
  TEST(lmjcore_txn_begin(env, LMJCORE_TXN_WRITE, &write_txn),
       "lmjcore_txn_begin (LMJCORE_TXN_WRITE)");

  // 7. 使用高级注册接口：只注册，不赋值
  const char *registered_member = "empty_field";
  TEST(lmjcore_obj_member_register(write_txn, obj_ptr,
                                   (const uint8_t *)registered_member,
                                   strlen(registered_member)),
       "lmjcore_obj_member_register (高级接口: 只注册不赋值)");

  // 8. 提交注册操作事务 -- 提交后才能读取到
  TEST(lmjcore_txn_commit(write_txn), "提交成员注册事务");
  write_txn = NULL;

  // 开启只读事务
  TEST(lmjcore_txn_begin(env, LMJCORE_TXN_READONLY, &read_txn),
       "开启只读--检查成员值是否存在");

  // 10. 检查注册后的状态 - 应为缺失值
  int exists;
  exists = lmjcore_obj_member_value_exist(read_txn, obj_ptr,
                                          (const uint8_t *)registered_member,
                                          strlen(registered_member));
  if (exists != 0) {
    printf("FAILED: 注册成员值应该不存在 (result: %d)\n", exists);
    lmjcore_txn_abort(write_txn);
    lmjcore_cleanup(env);
    return -1;
  }
  printf("OK: 注册成员值不存在 (符合预期)\n");

  TEST(lmjcore_txn_abort(read_txn), "释放读取事务的内存页");
  read_txn = NULL;

  TEST(lmjcore_txn_begin(env, LMJCORE_TXN_WRITE, &write_txn),
       "开启写事务--给注册成员赋值");
  // 9. 可以为注册的成员后续赋值
  const char *registered_value = "现在赋值";
  TEST(lmjcore_obj_member_put(
           write_txn, obj_ptr, (const uint8_t *)registered_member,
           strlen(registered_member), (const uint8_t *)registered_value,
           strlen(registered_value)),
       "为注册成员赋值");

  TEST(lmjcore_txn_commit(write_txn), "提交写事务--提交注册成员值");
  write_txn = NULL;

  TEST(lmjcore_txn_begin(env, LMJCORE_TXN_READONLY, &read_txn), "开启仅读事务--验证是否有值");
  // 10. 再次检查状态 - 现在应该有值了
  exists = lmjcore_obj_member_value_exist(read_txn, obj_ptr,
                                          (const uint8_t *)registered_member,
                                          strlen(registered_member));
  if (exists != 1) {
    printf("FAILED: 赋值后成员应存在值 (result: %d)\n", exists);
    lmjcore_txn_abort(write_txn);
    lmjcore_cleanup(env);
    return -1;
  }
  printf("OK: 赋值后成员值存在\n");

  TEST(lmjcore_txn_abort(read_txn), "释放读取内存页");
  read_txn = NULL;

  // ==================== 测试只读接口 ====================

  printf("\n4. 测试只读接口 (使用只读事务)...\n");
  TEST(lmjcore_txn_begin(env, LMJCORE_TXN_READONLY, &read_txn),
       "lmjcore_txn_begin (LMJCORE_TXN_READONLY)");

  // 12. 检查实体存在性
  exists = lmjcore_entity_exist(read_txn, obj_ptr);
  if (exists != 1) {
    printf("FAILED: 实体存在性检查失败 (result: %d)\n", exists);
    lmjcore_txn_abort(read_txn);
    lmjcore_cleanup(env);
    return -1;
  }
  printf("OK: lmjcore_entity_exist\n");

  // 13. 读取标准put接口创建的成员
  uint8_t value_buf[256];
  size_t value_len;
  TEST(lmjcore_obj_member_get(read_txn, obj_ptr, (const uint8_t *)member_name1,
                              strlen(member_name1), value_buf,
                              sizeof(value_buf), &value_len),
       "lmjcore_obj_member_get (读取标准接口创建的成员)");

  if (memcmp(value_buf, member_value1, value_len) != 0) {
    printf("FAILED: 读取的值不匹配\n");
    lmjcore_txn_abort(read_txn);
    lmjcore_cleanup(env);
    return -1;
  }
  printf("OK: 验证标准接口成员值正确\n");

  // 14. 读取高级注册接口创建并赋值的成员
  TEST(lmjcore_obj_member_get(
           read_txn, obj_ptr, (const uint8_t *)registered_member,
           strlen(registered_member), value_buf, sizeof(value_buf), &value_len),
       "lmjcore_obj_member_get (读取注册接口创建的成员)");

  if (memcmp(value_buf, registered_value, value_len) != 0) {
    printf("FAILED: 读取注册成员的值不匹配\n");
    lmjcore_txn_abort(read_txn);
    lmjcore_cleanup(env);
    return -1;
  }
  printf("OK: 验证注册成员值正确\n");

  // 15. 获取对象成员列表
  uint8_t member_list_buf[1024];
  lmjcore_result_arr *member_list;
  int list_ret = lmjcore_obj_member_list(read_txn, obj_ptr, member_list_buf,
                                         sizeof(member_list_buf), &member_list);
  if (list_ret == LMJCORE_SUCCESS && member_list) {
    printf("OK: lmjcore_obj_member_list (成员数: %zu)\n",
           member_list->element_count);
  } else {
    printf("INFO: 成员列表获取返回: %d (可能缓冲区不足)\n", list_ret);
  }

  lmjcore_txn_abort(read_txn);
  read_txn = NULL;

  // ==================== 测试数组操作 ====================

  printf("\n5. 测试数组操作 (使用写事务)...\n");
  TEST(lmjcore_txn_begin(env, LMJCORE_TXN_WRITE, &write_txn),
       "lmjcore_txn_begin (LMJCORE_TXN_WRITE)");

  // 16. 创建数组
  TEST(lmjcore_arr_create(write_txn, arr_ptr), "lmjcore_arr_create");

  // 17. 数组追加元素
  const char *element1 = "first_element";
  const char *element2 = "second_element";
  TEST(lmjcore_arr_append(write_txn, arr_ptr, (const uint8_t *)element1,
                          strlen(element1)),
       "lmjcore_arr_append (第一个元素)");

  TEST(lmjcore_arr_append(write_txn, arr_ptr, (const uint8_t *)element2,
                          strlen(element2)),
       "lmjcore_arr_append (第二个元素)");

  // 18. 提交数组创建事务
  TEST(lmjcore_txn_commit(write_txn), "提交数组创建事务");
  write_txn = NULL;

  // ==================== 测试其他工具函数 ====================

  printf("\n6. 测试其他工具函数...\n");
  TEST(lmjcore_txn_begin(env, LMJCORE_TXN_READONLY, &read_txn),
       "lmjcore_txn_begin (LMJCORE_TXN_READONLY)");

  // 19. 指针工具函数
  TEST(lmjcore_ptr_to_string(obj_ptr, str_buf, sizeof(str_buf)),
       "lmjcore_ptr_to_string");
  printf("对象指针字符串: %s\n", str_buf);

  TEST(lmjcore_ptr_to_string(arr_ptr, str_buf, sizeof(str_buf)),
       "lmjcore_ptr_to_string (数组指针)");
  printf("数组指针字符串: %s\n", str_buf);

  // 20. 对象统计
  size_t total_len, count;
  TEST(lmjcore_obj_stat_values(read_txn, obj_ptr, &total_len, &count),
       "lmjcore_obj_stat_values");
  printf("对象统计: %zu 个成员，总长度 %zu\n", count, total_len);

  // 21. 数组统计
  size_t arr_total_len, arr_count;
  TEST(lmjcore_arr_stat_element(read_txn, arr_ptr, &arr_total_len, &arr_count),
       "lmjcore_arr_stat_element");
  printf("数组统计: %zu 个元素，总长度 %zu\n", arr_count, arr_total_len);

  lmjcore_txn_abort(read_txn);
  read_txn = NULL;

  // ==================== 测试对象注册接口 ====================

  printf("\n7. 测试对象注册接口 (使用写事务)...\n");
  TEST(lmjcore_txn_begin(env, LMJCORE_TXN_WRITE, &write_txn),
       "lmjcore_txn_begin (LMJCORE_TXN_WRITE)");

  // 22. 创建自定义指针并注册
  uint8_t custom_ptr[LMJCORE_PTR_LEN];
  memset(custom_ptr, 0xBB, LMJCORE_PTR_LEN);
  custom_ptr[0] = 0x01; // 对象类型

  TEST(lmjcore_obj_register(write_txn, custom_ptr),
       "lmjcore_obj_register (自定义指针注册)");

  // 23. 验证注册的对象存在
  exists = lmjcore_entity_exist(write_txn, custom_ptr);
  if (exists != 1) {
    printf("FAILED: 注册对象存在性检查失败\n");
    lmjcore_txn_abort(write_txn);
    lmjcore_cleanup(env);
    return -1;
  }
  printf("OK: 注册对象存在\n");

  TEST(lmjcore_txn_commit(write_txn), "提交对象注册事务");
  write_txn = NULL;

  // ==================== 测试删除操作 ====================

  printf("\n8. 测试删除操作 (使用写事务)...\n");
  TEST(lmjcore_txn_begin(env, LMJCORE_TXN_WRITE, &write_txn),
       "lmjcore_txn_begin (LMJCORE_TXN_WRITE)");

  // 24. 删除数组成员
  TEST(lmjcore_arr_element_del(write_txn, arr_ptr, (const uint8_t *)element1,
                               strlen(element1)),
       "lmjcore_arr_element_del");

  // 25. 删除对象成员
  TEST(lmjcore_obj_member_del(write_txn, obj_ptr, (const uint8_t *)member_name1,
                              strlen(member_name1)),
       "lmjcore_obj_member_del");

  // 26. 删除整个数组
  TEST(lmjcore_arr_del(write_txn, arr_ptr), "lmjcore_arr_del");

  // 27. 删除整个对象
  TEST(lmjcore_obj_del(write_txn, obj_ptr), "lmjcore_obj_del");

  // 28. 删除注册的对象
  TEST(lmjcore_obj_del(write_txn, custom_ptr), "lmjcore_obj_del (注册对象)");

  TEST(lmjcore_txn_commit(write_txn), "提交删除事务");
  write_txn = NULL;

  // ==================== 清理环境 ====================

  printf("\n9. 测试环境清理...\n");
  TEST(lmjcore_cleanup(env), "lmjcore_cleanup");

  printf("\n========================================\n");
  printf("所有接口基本可用性测试通过！\n");
  printf("========================================\n");

  return 0;
}