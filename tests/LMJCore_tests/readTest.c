#include "../../Toolkit/ptr_uuid_gen/include/lmjcore_uuid_gen.h"
#include "../../core/include/lmjcore.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
  lmjcore_ptr ptr;
  lmjcore_ptr_from_string(argv[1], ptr);

  // 创建lmjcore环境变量
  lmjcore_env *env = NULL;
  lmjcore_txn *txn = NULL;

  // 初始化环境
  int rc = lmjcore_init("./lmjcore_db/", 1024 * 1024 * 100, LMJCORE_ENV_SAFE,
                        lmjcore_uuidv4_ptr_gen, NULL, &env);
  if (rc != LMJCORE_SUCCESS) {
    printf("初始化失败: %d\n", rc);
    return 1;
  }

  // 创建读取事务
  rc = lmjcore_txn_begin(env, NULL, LMJCORE_TXN_READONLY, &txn);
  if (rc != LMJCORE_SUCCESS) {
    printf("时间创建失败: %d\n", rc);
    return 1;
  }

  uint8_t result_buf[8192];
  lmjcore_result_obj *result;
  rc = lmjcore_obj_get(txn, ptr, result_buf, sizeof(result_buf), &result);

  if (rc == LMJCORE_SUCCESS) {
    for (size_t i = 0; i < result->member_count; i++) {
      lmjcore_member_descriptor desc = result->members[i];
      char *member_name = (char *)(result_buf + desc.member_name.value_offset);
      uint8_t *value = result_buf + desc.member_value.value_offset;

      printf("成员: %.*s, 值长度: %d\n", (int)desc.member_name.value_len,
             member_name, (int)desc.member_name.value_len);
      printf("%.*s : %.*s\n", (int)desc.member_name.value_len, member_name,
             (int)desc.member_value.value_len, value);
    }
  }

  lmjcore_txn_commit(txn);

  // 清理
  lmjcore_cleanup(env);
}
