#include "../../include/lmjcore.h"
#include <stdio.h>

int main() {
  lmjcore_ptr ptr;
  lmjcore_ptr_from_string("0192cc5c281d6f4ff7952aaa6408f76a6e", ptr);

  // 创建lmjcore环境变量
  lmjcore_env *env = NULL;
  lmjcore_txn *txn = NULL;

  // 初始化环境
  int rc = lmjcore_init("./lmjcore_db/", 1024 * 1024 * 100,
                        LMJCORE_WRITEMAP |       // 写内存映射
                            LMJCORE_NOSYNC |     // 不同步元数据
                            LMJCORE_NOMETASYNC | // 不同步数据
                            LMJCORE_MAPASYNC,
                        NULL, NULL, &env);
  if (rc != LMJCORE_SUCCESS) {
    printf("初始化失败: %d\n", rc);
    return 1;
  }

  // 创建读取事务
  rc = lmjcore_txn_begin(env, LMJCORE_TXN_READONLY, &txn);
  if (rc != LMJCORE_SUCCESS) {
    printf("时间创建失败: %d\n", rc);
    return 1;
  }

  uint8_t result_buf[8192];
  lmjcore_result *result;
  rc = lmjcore_obj_get(txn, ptr, LMJCORE_MODE_LOOSE, result_buf,
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
}
