#include "lmjcore_config.h"
#include <stdio.h>
#include <string.h>

const lmjcore_ptr *lmjcore_config_object_ptr() {
  return (const lmjcore_ptr *)&LMJCORE_CONFIG_OBJECT_PTR;
}

// 确保配置对象可用
//
// 注意：空实体在语义上不存在。配置对象使用固定指针，其存在性同样由 set 库
// 决定——在首次写入配置项之前它并不存在，而 lmjcore_obj_member_put 本身
// 就会创建它，因此这里无需（也无法）预先生成一个空对象。
int lmjcore_config_object_ensure(lmjcore_txn *txn) {
  if (!txn) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  return LMJCORE_SUCCESS;
}

int lmjcore_config_object_exist(lmjcore_txn *txn) {
  if (!txn) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  return lmjcore_entity_exist(txn, LMJCORE_CONFIG_OBJECT_PTR);
}

int lmjcore_config_set(lmjcore_txn *txn, const uint8_t *key, size_t key_len,
                       const uint8_t *value, size_t value_len) {
  if (!txn || !key || key_len == 0) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  // 检查成员名长度限制
  if (key_len > LMJCORE_MAX_MEMBER_NAME_LEN) {
    return LMJCORE_ERROR_MEMBER_TOO_LONG;
  }

  // 确保配置对象存在
  int ret = lmjcore_config_object_ensure(txn);
  if (ret != LMJCORE_SUCCESS) {
    return ret;
  }

  // 设置配置项
  return lmjcore_obj_member_put(txn, LMJCORE_CONFIG_OBJECT_PTR, key, key_len,
                                value, value_len);
}

int lmjcore_config_get(lmjcore_txn *txn, const uint8_t *key, size_t key_len,
                       uint8_t *value_buf, size_t value_buf_size,
                       size_t *value_size) {
  if (!txn || !key || key_len == 0 || !value_buf || !value_size) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  // 检查成员名长度限制
  if (key_len > LMJCORE_MAX_MEMBER_NAME_LEN) {
    return LMJCORE_ERROR_MEMBER_TOO_LONG;
  }

  // 检查配置对象是否存在
  int exist = lmjcore_config_object_exist(txn);
  if (exist < 0) {
    return exist;
  }
  if (exist == 0) {
    return LMJCORE_ERROR_ENTITY_NOT_FOUND;
  }

  // 获取配置项
  return lmjcore_obj_member_get(txn, LMJCORE_CONFIG_OBJECT_PTR, key, key_len,
                                value_buf, value_buf_size, value_size);
}

int lmjcore_config_get_all(lmjcore_txn *txn, uint8_t *result_buf,
                           size_t result_buf_size,
                           lmjcore_result_obj **result_head) {
  if (!txn || !result_buf || !result_head) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  // 检查配置对象是否存在
  int exist = lmjcore_config_object_exist(txn);
  if (exist < 0) {
    return exist;
  }
  if (exist == 0) {
    return LMJCORE_ERROR_ENTITY_NOT_FOUND;
  }

  // 获取完整配置对象
  return lmjcore_obj_get(txn, LMJCORE_CONFIG_OBJECT_PTR, result_buf,
                         result_buf_size, result_head);
}