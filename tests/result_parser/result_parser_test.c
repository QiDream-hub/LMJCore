#include "../../Toolkit/ptr_uuid_gen/include/lmjcore_uuid_gen.h"
#include "../../Toolkit/result_parser/include/result_parser.h"
#include "../../core/include/lmjcore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 测试配置
#define TEST_DB_PATH "./test_db"
#define TEST_MAP_SIZE (1024 * 1024) // 1MB

// 测试辅助函数
void print_hex(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    printf("%02x", data[i]);
  }
}

void print_test_result(const char *test_name, int result) {
  printf("%-40s: %s\n", test_name, result == LMJCORE_SUCCESS ? "PASS" : "FAIL");
}

// 测试 1: 基本对象操作
int test_basic_object_operations(lmjcore_env *env) {
  lmjcore_txn *txn;
  lmjcore_ptr obj_ptr;
  int ret;

  printf("\n=== 测试 1: 基本对象操作 ===\n");

  // 开始写事务
  ret = lmjcore_txn_begin(env, LMJCORE_TXN_WRITE, &txn);
  if (ret != LMJCORE_SUCCESS) {
    printf("开启事务失败: %d\n", ret);
    return ret;
  }

  // 创建对象
  ret = lmjcore_obj_create(txn, obj_ptr);
  if (ret != LMJCORE_SUCCESS) {
    printf("创建对象失败: %d\n", ret);
    lmjcore_txn_abort(txn);
    return ret;
  }

  char ptr_str[35];
  lmjcore_ptr_to_string(obj_ptr, ptr_str, sizeof(ptr_str));
  printf("创建对象: %s\n", ptr_str);

  // 设置对象成员
  const char *username = "test_user";
  const char *email = "user@example.com";
  const int age = 25;

  ret = lmjcore_obj_member_put(txn, obj_ptr, (const uint8_t *)"username", 8,
                               (const uint8_t *)username, strlen(username));
  print_test_result("设置 username 成员", ret);

  ret = lmjcore_obj_member_put(txn, obj_ptr, (const uint8_t *)"email", 5,
                               (const uint8_t *)email, strlen(email));
  print_test_result("设置 email 成员", ret);

  ret = lmjcore_obj_member_put(txn, obj_ptr, (const uint8_t *)"age", 3,
                               (const uint8_t *)&age, sizeof(age));
  print_test_result("设置 age 成员", ret);

  // 提交事务
  ret = lmjcore_txn_commit(txn);
  if (ret != LMJCORE_SUCCESS) {
    printf("提交事务失败: %d\n", ret);
    return ret;
  }

  // 开始读事务验证
  ret = lmjcore_txn_begin(env, LMJCORE_TXN_READONLY, &txn);
  if (ret != LMJCORE_SUCCESS) {
    printf("开启读事务失败: %d\n", ret);
    return ret;
  }

  // 获取对象数据
  uint8_t result_buf[4096];
  lmjcore_result *result;

  ret = lmjcore_obj_get(txn, obj_ptr, LMJCORE_MODE_LOOSE, result_buf,
                        sizeof(result_buf), &result);
  print_test_result("获取对象数据", ret);

  if (ret == LMJCORE_SUCCESS) {
    // 测试对象解析器
    size_t member_count = lmjcore_parser_obj_member_count(result);
    printf("对象成员数量: %zu\n", member_count);

    // 查找特定成员
    const uint8_t *value_data;
    size_t value_len;

    ret = lmjcore_parser_obj_find_member(result, result_buf,
                                         (const uint8_t *)"username", 8,
                                         &value_data, &value_len);
    if (ret == LMJCORE_SUCCESS) {
      printf("找到 username: %.*s\n", (int)value_len, value_data);
    }

    // 遍历所有成员
    printf("遍历对象成员:\n");
    for (size_t i = 0; i < member_count; i++) {
      const uint8_t *name_data, *value_data;
      size_t name_len, value_len;

      ret = lmjcore_parser_obj_get_member(result, result_buf, i, &name_data,
                                          &name_len, &value_data, &value_len);
      if (ret == LMJCORE_SUCCESS) {
        printf("  [%zu] %.*s = ", i, (int)name_len, name_data);

        if (name_len == 3 && memcmp(name_data, "age", 3) == 0) {
          // 年龄是整数
          int age_value = *(int *)value_data;
          printf("%d\n", age_value);
        } else {
          // 其他成员是字符串
          printf("%.*s\n", (int)value_len, value_data);
        }
      }
    }
  }

  lmjcore_txn_abort(txn);
  return LMJCORE_SUCCESS;
}

// 测试 2: 数组操作
int test_array_operations(lmjcore_env *env) {
  lmjcore_txn *txn;
  lmjcore_ptr arr_ptr;
  int ret;

  printf("\n=== 测试 2: 数组操作 ===\n");

  // 开始写事务
  ret = lmjcore_txn_begin(env, LMJCORE_TXN_WRITE, &txn);
  if (ret != LMJCORE_SUCCESS) {
    printf("开启事务失败: %d\n", ret);
    return ret;
  }

  // 创建数组
  ret = lmjcore_arr_create(txn, arr_ptr);
  if (ret != LMJCORE_SUCCESS) {
    printf("创建数组失败: %d\n", ret);
    lmjcore_txn_abort(txn);
    return ret;
  }

  char ptr_str[35];
  lmjcore_ptr_to_string(arr_ptr, ptr_str, sizeof(ptr_str));
  printf("创建数组: %s\n", ptr_str);

  // 向数组添加元素
  const char *elements[] = {"first", "second", "third", "fourth"};
  for (int i = 0; i < 4; i++) {
    ret = lmjcore_arr_append(txn, arr_ptr, (const uint8_t *)elements[i],
                             strlen(elements[i]));
    printf("添加元素 '%s': %s\n", elements[i],
           ret == LMJCORE_SUCCESS ? "成功" : "失败");
  }

  // 提交事务
  ret = lmjcore_txn_commit(txn);
  if (ret != LMJCORE_SUCCESS) {
    printf("提交事务失败: %d\n", ret);
    return ret;
  }

  // 开始读事务验证
  ret = lmjcore_txn_begin(env, LMJCORE_TXN_READONLY, &txn);
  if (ret != LMJCORE_SUCCESS) {
    printf("开启读事务失败: %d\n", ret);
    return ret;
  }

  // 获取数组数据
  uint8_t result_buf[4096];
  lmjcore_result *result;

  ret = lmjcore_arr_get(txn, arr_ptr, LMJCORE_MODE_LOOSE, result_buf,
                        sizeof(result_buf), &result);
  print_test_result("获取数组数据", ret);

  if (ret == LMJCORE_SUCCESS) {
    // 测试数组解析器
    size_t element_count = lmjcore_parser_arr_element_count(result);
    printf("数组元素数量: %zu\n", element_count);

    // 遍历数组元素
    printf("遍历数组元素:\n");
    for (size_t i = 0; i < element_count; i++) {
      const uint8_t *element_data;
      size_t element_len;

      ret = lmjcore_parser_arr_get_element(result, result_buf, i, &element_data,
                                           &element_len);
      if (ret == LMJCORE_SUCCESS) {
        printf("  [%zu] %.*s\n", i, (int)element_len, element_data);
      }
    }

    // 测试数组查找
    size_t found_index;
    ret = lmjcore_parser_arr_find_element(
        result, result_buf, (const uint8_t *)"third", 5, &found_index);
    if (ret == LMJCORE_SUCCESS) {
      printf("找到 'third' 在索引位置: %zu\n", found_index);
    } else {
      printf("未找到 'third'\n");
    }

    // 查找不存在的元素
    ret = lmjcore_parser_arr_find_element(
        result, result_buf, (const uint8_t *)"nonexistent", 11, &found_index);
    if (ret == LMJCORE_ERROR_ENTITY_NOT_FOUND) {
      printf("正确报告未找到 'nonexistent'\n");
    }
  }

  lmjcore_txn_abort(txn);
  return LMJCORE_SUCCESS;
}

// 测试 3: 错误处理
int test_error_handling(lmjcore_env *env) {
  lmjcore_txn *txn;
  lmjcore_ptr obj_ptr;
  int ret;

  printf("\n=== 测试 3: 错误处理 ===\n");

  // 开始写事务
  ret = lmjcore_txn_begin(env, LMJCORE_TXN_WRITE, &txn);
  if (ret != LMJCORE_SUCCESS) {
    printf("开启事务失败: %d\n", ret);
    return ret;
  }

  // 创建对象
  ret = lmjcore_obj_create(txn, obj_ptr);
  if (ret != LMJCORE_SUCCESS) {
    printf("创建对象失败: %d\n", ret);
    lmjcore_txn_abort(txn);
    return ret;
  }

  // 注册成员但不设置值（创建缺失值）
  ret = lmjcore_obj_member_register(txn, obj_ptr,
                                    (const uint8_t *)"missing_field", 13);
  print_test_result("注册缺失成员", ret);

  // 设置一个正常成员
  ret = lmjcore_obj_member_put(txn, obj_ptr, (const uint8_t *)"normal_field",
                               12, (const uint8_t *)"has_value", 9);
  print_test_result("设置正常成员", ret);

  // 提交事务
  ret = lmjcore_txn_commit(txn);
  if (ret != LMJCORE_SUCCESS) {
    printf("提交事务失败: %d\n", ret);
    return ret;
  }

  // 测试严格模式
  ret = lmjcore_txn_begin(env, LMJCORE_TXN_READONLY, &txn);
  if (ret != LMJCORE_SUCCESS) {
    printf("开启读事务失败: %d\n", ret);
    return ret;
  }

  uint8_t result_buf[4096];
  lmjcore_result *result;

  printf("测试严格模式:\n");
  ret = lmjcore_obj_get(txn, obj_ptr, LMJCORE_MODE_STRICT, result_buf,
                        sizeof(result_buf), &result);

  if (ret != LMJCORE_SUCCESS) {
    printf("严格模式正确拒绝缺失值: %d\n", ret);

    // 检查错误信息
    size_t error_count = lmjcore_parser_error_count(result);
    printf("错误数量: %zu\n", error_count);

    for (size_t i = 0; i < error_count; i++) {
      const lmjcore_read_error *error;
      if (lmjcore_parser_get_error(result, i, &error) == LMJCORE_SUCCESS) {
        printf("  错误[%zu]: code=%d\n", i, error->code);
      }
    }

    // 检查特定错误
    if (lmjcore_parser_has_error(result, LMJCORE_READERR_MEMBER_MISSING)) {
      printf("正确检测到成员缺失错误\n");
    }
  }

  // 测试宽松模式
  printf("测试宽松模式:\n");
  ret = lmjcore_obj_get(txn, obj_ptr, LMJCORE_MODE_LOOSE, result_buf,
                        sizeof(result_buf), &result);

  if (ret == LMJCORE_SUCCESS) {
    printf("宽松模式成功处理缺失值\n");

    size_t member_count = lmjcore_parser_obj_member_count(result);
    printf("宽松模式返回成员数量: %zu\n", member_count);

    // 检查错误信息（宽松模式可能有警告）
    size_t error_count = lmjcore_parser_error_count(result);
    if (error_count > 0) {
      printf("宽松模式报告 %zu 个警告:\n", error_count);
      for (size_t i = 0; i < error_count; i++) {
        const lmjcore_read_error *error;
        if (lmjcore_parser_get_error(result, i, &error) == LMJCORE_SUCCESS) {
          printf("  警告[%zu]: code=%d\n", i, error->code);
        }
      }
    }
  }

  lmjcore_txn_abort(txn);
  return LMJCORE_SUCCESS;
}

// 测试 4: 边界情况测试
int test_edge_cases(lmjcore_env *env) {
  printf("\n=== 测试 4: 边界情况 ===\n");

  lmjcore_txn *txn;
  int ret;

  // 测试空对象
  ret = lmjcore_txn_begin(env, LMJCORE_TXN_READONLY, &txn);
  if (ret != LMJCORE_SUCCESS) {
    printf("开启事务失败: %d\n", ret);
    return ret;
  }

  // 创建空对象指针（仅用于测试）
  lmjcore_ptr empty_obj;
  memset(empty_obj, 0, LMJCORE_PTR_LEN);
  empty_obj[0] = LMJCORE_OBJ;

  uint8_t result_buf[1024];
  lmjcore_result *result;

  // 测试不存在的对象
  ret = lmjcore_obj_get(txn, empty_obj, LMJCORE_MODE_LOOSE, result_buf,
                        sizeof(result_buf), &result);
  if (ret == LMJCORE_ERROR_ENTITY_NOT_FOUND) {
    printf("正确报告不存在的对象\n");
  }

  // 测试无效参数
  ret = lmjcore_parser_obj_find_member(NULL, result_buf,
                                       (const uint8_t *)"test", 4, NULL, NULL);
  if (ret == LMJCORE_ERROR_INVALID_PARAM) {
    printf("正确处理无效参数\n");
  }

  lmjcore_txn_abort(txn);
  return LMJCORE_SUCCESS;
}

int main() {
  lmjcore_env *env = NULL;
  int ret;

  printf("LMJCore Result Parser 测试程序\n");
  printf("==============================\n");

  // 初始化 LMJCore 环境
  ret = lmjcore_init(TEST_DB_PATH, TEST_MAP_SIZE,
                     LMJCORE_FLAGS_NOSUBDIR | LMJCORE_FLAGS_SAFE_SYNC,
                     lmjcore_uuidv4_ptr_gen, NULL, &env);

  if (ret != LMJCORE_SUCCESS) {
    printf("初始化 LMJCore 环境失败: %d\n", ret);
    return 1;
  }

  printf("LMJCore 环境初始化成功\n");

  // 运行测试
  int all_tests_passed = 1;

  if (test_basic_object_operations(env) != LMJCORE_SUCCESS) {
    all_tests_passed = 0;
  }

  if (test_array_operations(env) != LMJCORE_SUCCESS) {
    all_tests_passed = 0;
  }

  if (test_error_handling(env) != LMJCORE_SUCCESS) {
    all_tests_passed = 0;
  }

  if (test_edge_cases(env) != LMJCORE_SUCCESS) {
    all_tests_passed = 0;
  }

  // 清理环境
  lmjcore_cleanup(env);

  printf("\n==============================\n");
  if (all_tests_passed) {
    printf("所有测试通过！\n");
    return 0;
  } else {
    printf("部分测试失败！\n");
    return 1;
  }
}