#include "../../include/lmjcore.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// 示例：创建对象并写入数据

int main() {
  lmjcore_env *env = NULL;
  lmjcore_txn *txn = NULL;

  // 初始化环境
  int rc =
      lmjcore_init("./lmjcore_db/", 1024 * 1024 * 100, 0, NULL, NULL, &env);
  if (rc != LMJCORE_SUCCESS) {
    printf("初始化失败: %d\n", rc);
    return 1;
  }

  // 开始写事务
  rc = lmjcore_txn_begin(env, LMJCORE_TXN_WRITE, &txn);
  if (rc != LMJCORE_SUCCESS) {
    printf("事务开始失败: %d\n", rc);
    lmjcore_cleanup(env);
    return 1;
  }

  lmjcore_ptr arr_ptr;
  // 创建数组
  rc = lmjcore_arr_create(txn, arr_ptr);
  if (rc != LMJCORE_SUCCESS) {
    printf("数组创建失败");
    lmjcore_txn_abort(txn);
    lmjcore_cleanup(env);
  }
  rc = lmjcore_arr_append(txn, arr_ptr, (uint8_t *)"const uint8_t *value",
                          sizeof("const uint8_t *value"));
  if (rc != LMJCORE_SUCCESS) {
    printf("数组提交元素失败");
    lmjcore_txn_abort(txn);
    lmjcore_cleanup(env);
  }

  // 创建对象
  lmjcore_ptr obj_ptr;
  rc = lmjcore_obj_create(txn, obj_ptr);
  if (rc != LMJCORE_SUCCESS) {
    printf("对象创建失败: %d\n", rc);
    lmjcore_txn_abort(txn);
    lmjcore_cleanup(env);
    return 1;
  }

  // 转换为可读字符串
  char ptr_str[35];
  lmjcore_ptr_to_string(obj_ptr, ptr_str, sizeof(ptr_str));
  printf("创建对象: %s\n", ptr_str);

  // 写入成员数据
  const char *name = "John Doe";
  const char *email = "john@example.com";

  rc = lmjcore_obj_member_put(txn, obj_ptr, (uint8_t *)"name", 4, (uint8_t *)name,
                       strlen(name));
  rc = lmjcore_obj_member_put(txn, obj_ptr, (uint8_t *)"email", 5, (uint8_t *)email,
                       strlen(email));

  // 提交事务
  rc = lmjcore_txn_commit(txn);

  // 读取数据
  lmjcore_txn_begin(env, LMJCORE_TXN_READONLY, &txn);

  uint8_t result_buf[8192];
  lmjcore_result *result;
  rc = lmjcore_obj_get(txn, obj_ptr, LMJCORE_MODE_LOOSE, result_buf,
                       sizeof(result_buf), &result);

  if (rc == LMJCORE_SUCCESS) {
    for (size_t i = 0; i < result->count; i++) {
      lmjcore_obj_descriptor *desc = &result->descriptor.object_descriptors[i];
      char *member_name = (char *)(result_buf + desc->name_offset);
      uint8_t *value = result_buf + desc->value_offset;

      printf("成员: %.*s, 值长度: %d\n", desc->name_len, member_name,
             desc->value_len);
      printf("%.*s : %.*s\n", desc->name_len, member_name, desc->value_len,
             value);
    }
  }

  lmjcore_txn_commit(txn);

  // 清理
  lmjcore_cleanup(env);
  //   printf("操作成功完成\n");

  return 0;
}