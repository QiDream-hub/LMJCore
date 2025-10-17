#include "../../include/lmjcore.h"
#include <complex.h>
#include <lmdb.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <uuid/uuid.h>

#define MAIN_DB_NAME "main"
#define ARR_DB_NAME "arr"
#define MAX_MEMBER_LENGTH 493 // 511 - 17 - 1

/*
 *==========================================
 *内部结构
 *==========================================
 */
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
  lmjcore_txn_type type;
};

// 构建一个内部链表存储成员信息
typedef struct MDB_val_list {
  MDB_val val;
  struct MDB_val_list *next;
} MDB_val_list;

/*
 *==========================================
 *内部工具函数
 *==========================================
 */

/**
 * @brief 添加一个节点,不检查是否存在
 *
 * @param head 列表头
 * @param value 要存储的值
 * @return true
 * @return false
 */
static bool mdb_val_list_append(MDB_val_list **head, MDB_val value) {
  if (!head)
    return false;
  MDB_val_list *node = malloc(sizeof(MDB_val_list));
  if (!node)
    return false;
  node->val = value;
  node->next = *head;
  *head = node;
  return true;
}

/**
 * @brief 删除一个匹配的成员
 *
 * @param head 列表头
 * @param target 目标成员
 * @return true
 * @return false
 */
static bool mdb_val_list_remove(MDB_val_list **head, const MDB_val *target) {
  if (!head || !*head || !target)
    return false;

  MDB_val_list *prev = NULL;
  MDB_val_list *curr = *head;

  while (curr) {
    if (curr->val.mv_size == target->mv_size &&
        memcmp(curr->val.mv_data, target->mv_data, target->mv_size) == 0) {
      // 找到匹配项
      if (prev) {
        prev->next = curr->next;
      } else {
        *head = curr->next; // 移除头节点
      }
      free(curr);
      return true;
    }
    prev = curr;
    curr = curr->next;
  }
  return false;
}

/**
 * @brief 释放整个列表
 *
 * @param head 列表头部
 */
static void mdb_val_list_free_all(MDB_val_list *head) {
  while (head) {
    MDB_val_list *next = head->next;
    free(head);
    head = next;
  }
}

/**
 * @brief 检查MDB_val的key是否以指定的对象指针开头
 * @param obj_ptr 对象指针
 * @param key LMDB的key值
 * @return true表示属于该对象，false表示不属于
 */
static inline bool OBJ_KEY_PREFIX(const lmjcore_ptr *obj_ptr, MDB_val key) {
  return (key.mv_size >= 17) && (memcmp(key.mv_data, obj_ptr->data, 17) == 0);
}

/**
 * @brief 向结果对象添加错误信息
 * @param result 结果对象指针
 * @param code 错误代码
 * @param entity_ptr 实体指针
 * @param member_offset 成员偏移量（如果是成员错误）
 * @param member_len 成员长度（如果是成员错误）
 * @param error_code 底层错误代码
 * @return 是否成功添加错误（如果错误数已达上限则返回false）
 */
static bool add_error_to_result(lmjcore_result *result,
                                lmjcore_read_error_code code,
                                const lmjcore_ptr *entity_ptr,
                                uint32_t member_offset, uint16_t member_len,
                                int error_code) {
  if (result->error_count >= LMJCORE_MAX_READ_ERRORS) {
    return false;
  }

  lmjcore_read_error *error = &result->errors[result->error_count];
  error->code = code;
  error->entity_ptr = *entity_ptr;

  switch (code) {
  case LMJCORE_READERR_MEMBER_MISSING:
    error->context.member.member_offset = member_offset;
    error->context.member.member_len = member_len;
    break;
  case LMJCORE_READERR_BUFFER_TOO_SMALL:
  case LMJCORE_READERR_LMDB_FAILED:
  case LMJCORE_READERR_MEMORY_ALLOCATION_FAILED:
    error->context.error_code.code = error_code;
    break;
  case LMJCORE_READERR_ENTITY_NOT_FOUND:
  case LMJCORE_READERR_NONE:
  default:
    // 不需要额外的上下文信息
    break;
  }

  result->error_count++;
  return true;
}

/**
 * @brief 添加缓冲区不足错误
 */
static bool add_buffer_error(lmjcore_result *result,
                             const lmjcore_ptr *entity_ptr) {
  return add_error_to_result(result, LMJCORE_READERR_BUFFER_TOO_SMALL,
                             entity_ptr, 0, 0, LMJCORE_ERROR_BUFFER_TOO_SMALL);
}

/**
 * @brief 添加实体未找到错误
 */
static bool add_entity_not_found_error(lmjcore_result *result,
                                       const lmjcore_ptr *entity_ptr) {
  return add_error_to_result(result, LMJCORE_READERR_ENTITY_NOT_FOUND,
                             entity_ptr, 0, 0, 0);
}

/**
 * @brief 添加LMDB操作错误
 */
static bool add_lmdb_error(lmjcore_result *result,
                           const lmjcore_ptr *entity_ptr, int error_code) {
  return add_error_to_result(result, LMJCORE_READERR_LMDB_FAILED, entity_ptr, 0,
                             0, error_code);
}

/**
 * @brief 添加成员缺失错误
 */
static bool add_member_missing_error(lmjcore_result *result,
                                     const lmjcore_ptr *entity_ptr,
                                     uint32_t member_offset,
                                     uint16_t member_len) {
  return add_error_to_result(result, LMJCORE_READERR_MEMBER_MISSING, entity_ptr,
                             member_offset, member_len, 0);
}

static int add_audit(lmjcore_audit_report *report, uint8_t *buf,
                     lmjcore_audit_error_code error, const MDB_val *member_name,
                     const lmjcore_ptr *obj_ptr, size_t *data_offset,
                     size_t *report_descriptor_offset, size_t total_buf_size) {
  if (!report || !buf || !member_name || !obj_ptr || !data_offset ||
      !report_descriptor_offset) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  size_t needed = member_name->mv_size;
  size_t desc_size = sizeof(lmjcore_audit_descriptor);

  // 检查描述符区是否有空间
  if (*report_descriptor_offset + desc_size > *data_offset) {
    return LMJCORE_ERROR_BUFFER_TOO_SMALL;
  }

  // 检查数据区是否有空间（从后往前写）
  if (*data_offset < needed) {
    return LMJCORE_ERROR_BUFFER_TOO_SMALL;
  }

  // 写入成员名（从缓冲区末尾向前）
  *data_offset -= needed;
  memcpy(buf + *data_offset, member_name->mv_data, needed);

  // 写入审计描述符
  lmjcore_audit_descriptor *desc =
      &report->audit_descriptor[report->audit_cont];
  desc->error = error;
  desc->ptr = *obj_ptr;
  desc->member_offset = *data_offset;
  desc->member_len = (uint16_t)needed;

  report->audit_cont++;
  *report_descriptor_offset += desc_size;

  return LMJCORE_SUCCESS;
}

// 默认指针生成器 (UUIDv4)
static int default_ptr_generator(void *ctx, uint8_t out[17]) {
  uuid_t uuid;
  uuid_generate(uuid);

  // 设置类型前缀为对象类型（上层可覆盖）
  out[0] = LMJCORE_OBJ;

  // 复制UUID到后16字节
  memcpy(out + 1, uuid, 16);
  return LMJCORE_SUCCESS;
}

// 初始化函数
int lmjcore_init(const char *path, size_t map_size,
                 lmjcore_ptr_generator_fn ptr_gen, void *ptr_gen_ctx,
                 lmjcore_env **env) {
  if (!path || !env) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  lmjcore_env *new_env = calloc(1, sizeof(lmjcore_env));
  if (!new_env) {
    return LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED;
  }

  // 设置指针生成器
  if (ptr_gen) {
    new_env->ptr_generator = ptr_gen;
    new_env->ptr_gen_ctx = ptr_gen_ctx;
  } else {
    new_env->ptr_generator = default_ptr_generator;
    new_env->ptr_gen_ctx = NULL;
  }

  // 初始化 LMDB 环境
  int rc = mdb_env_create(&new_env->mdb_env);
  if (rc != MDB_SUCCESS) {
    free(new_env);
    return rc;
  }

  // 设置映射大小
  rc = mdb_env_set_mapsize(new_env->mdb_env, map_size);
  if (rc != MDB_SUCCESS) {
    mdb_env_close(new_env->mdb_env);
    free(new_env);
    return rc;
  }

  // 设置数据库数量(main,arr)
  rc = mdb_env_set_maxdbs(new_env->mdb_env, 2);
  if (rc != MDB_SUCCESS) {
    mdb_env_close(new_env->mdb_env);
    free(new_env);
    return rc;
  }

  // 打开mdb的环境
  rc = mdb_env_open(new_env->mdb_env, path, 0, 0664);
  if (rc != MDB_SUCCESS) {
    mdb_env_close(new_env->mdb_env);
    free(new_env);
    return rc;
  }

  // 打开数据库
  MDB_txn *txn;
  rc = mdb_txn_begin(new_env->mdb_env, NULL, 0, &txn);
  if (rc != MDB_SUCCESS) {
    mdb_env_close(new_env->mdb_env);
    free(new_env);
    return rc;
  }
  // 打开mian数据库
  rc = mdb_dbi_open(txn, MAIN_DB_NAME, MDB_CREATE, &new_env->main_dbi);
  if (rc != MDB_SUCCESS) {
    mdb_txn_abort(txn);
    mdb_env_close(new_env->mdb_env);
    free(new_env);
    return rc;
  }

  // 打开arr数据库
  rc = mdb_dbi_open(txn, ARR_DB_NAME, MDB_CREATE | MDB_DUPSORT,
                    &new_env->arr_dbi);
  if (rc != MDB_SUCCESS) {
    mdb_txn_abort(txn);
    mdb_env_close(new_env->mdb_env);
    free(new_env);
    return rc;
  }
  mdb_txn_commit(txn);
  *env = new_env;

  return LMJCORE_SUCCESS;
}

// 清理函数
int lmjcore_cleanup(lmjcore_env *env) {
  if (!env) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  mdb_dbi_close(env->mdb_env, env->main_dbi);
  mdb_dbi_close(env->mdb_env, env->arr_dbi);
  mdb_env_close(env->mdb_env);
  free(env);

  return LMJCORE_SUCCESS;
}

// 事务开始
int lmjcore_txn_begin(lmjcore_env *env, lmjcore_txn_type type,
                      lmjcore_txn **txn) {
  if (!env || !txn) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  lmjcore_txn *new_txn = calloc(1, sizeof(lmjcore_txn));
  if (!new_txn) {
    return LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED;
  }

  new_txn->env = env;
  new_txn->type = type;

  int mdb_flags = (type == LMJCORE_TXN_READONLY) ? MDB_RDONLY : 0;
  int rc = mdb_txn_begin(env->mdb_env, NULL, mdb_flags, &new_txn->mdb_txn);
  if (rc != MDB_SUCCESS) {
    free(new_txn);
    return rc;
  }

  *txn = new_txn;
  return LMJCORE_SUCCESS;
}

// 事务提交
int lmjcore_txn_commit(lmjcore_txn *txn) {
  if (!txn) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  int rc = mdb_txn_commit(txn->mdb_txn);
  free(txn);

  if (rc != MDB_SUCCESS) {
    return rc;
  }

  return LMJCORE_SUCCESS;
}

// 事务中止
int lmjcore_txn_abort(lmjcore_txn *txn) {
  if (!txn) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  mdb_txn_abort(txn->mdb_txn);
  free(txn);

  return LMJCORE_SUCCESS;
}

// 创建对象
int lmjcore_obj_create(lmjcore_txn *txn, lmjcore_ptr *ptr) {
  if (!txn || !ptr || txn->type != LMJCORE_TXN_WRITE) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  // 生成指针
  int rc = txn->env->ptr_generator(txn->env->ptr_gen_ctx, ptr->data);
  if (rc != LMJCORE_SUCCESS) {
    return rc;
  }

  // 确保指针类型为对象
  ptr->data[0] = LMJCORE_OBJ;

  return LMJCORE_SUCCESS;
}

// 对象成员写入
int lmjcore_obj_put(lmjcore_txn *txn, const lmjcore_ptr *obj_ptr,
                    const uint8_t *member_name, size_t member_name_len,
                    const uint8_t *value, size_t value_len) {
  if (!txn || !obj_ptr || !member_name || txn->type != LMJCORE_TXN_WRITE) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  // 检查成员名长度
  if (member_name_len > MAX_MEMBER_LENGTH) {
    return LMJCORE_ERROR_MEMBER_TOO_LONG;
  }

  // 在 arr 数据库中注册成员名（如果尚未注册）
  MDB_val arr_key = {.mv_size = 17, .mv_data = (void *)obj_ptr->data};
  MDB_val arr_val = {.mv_size = member_name_len,
                     .mv_data = (void *)member_name};

  // 使用lmdb数据库的原生检查插入
  int rc = mdb_put(txn->mdb_txn, txn->env->arr_dbi, &arr_key, &arr_val,
                   MDB_NODUPDATA);
  // 有重复的值
  if (rc == MDB_KEYEXIST || rc == MDB_SUCCESS) {
    // 构建主键：对象指针 + 成员名
    size_t key_size = 17 + member_name_len;
    uint8_t *key = malloc(key_size);
    if (!key) {
      return LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED;
    }

    memcpy(key, obj_ptr->data, 17);
    memcpy(key + 17, member_name, member_name_len);

    // 写入 main 数据库
    MDB_val mdb_key = {.mv_size = key_size, .mv_data = key};
    MDB_val mdb_val = {.mv_size = value_len, .mv_data = (void *)value};

    rc = mdb_put(txn->mdb_txn, txn->env->main_dbi, &mdb_key, &mdb_val, 0);
    free(key);

    if (rc != MDB_SUCCESS) {
      return rc;
    }
  }

  return rc;
}

// 指针转字符串
int lmjcore_ptr_to_string(const lmjcore_ptr *ptr, char *str_buf,
                          size_t buf_size) {
  if (!ptr || !str_buf || buf_size < 34) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  const char hex_chars[] = "0123456789abcdef";
  for (int i = 0; i < 17; i++) {
    uint8_t byte = ptr->data[i];
    str_buf[i * 2] = hex_chars[(byte >> 4) & 0x0F];
    str_buf[i * 2 + 1] = hex_chars[byte & 0x0F];
  }
  str_buf[34] = '\0';

  return LMJCORE_SUCCESS;
}

// 字符串转指针
int lmjcore_ptr_from_string(const char *str, lmjcore_ptr *ptr) {
  if (!str || !ptr || strlen(str) != 34) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  for (int i = 0; i < 17; i++) {
    char high = str[i * 2];
    char low = str[i * 2 + 1];

    if (high >= '0' && high <= '9')
      high = high - '0';
    else if (high >= 'a' && high <= 'f')
      high = high - 'a' + 10;
    else if (high >= 'A' && high <= 'F')
      high = high - 'A' + 10;
    else
      return LMJCORE_ERROR_INVALID_POINTER;

    if (low >= '0' && low <= '9')
      low = low - '0';
    else if (low >= 'a' && low <= 'f')
      low = low - 'a' + 10;
    else if (low >= 'A' && low <= 'F')
      low = low - 'A' + 10;
    else
      return LMJCORE_ERROR_INVALID_POINTER;

    ptr->data[i] = (high << 4) | low;
  }

  return LMJCORE_SUCCESS;
}

// 存在性检查
int lmjcore_exist(lmjcore_txn *txn, const lmjcore_ptr *ptr) {
  if (!txn || !ptr) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  MDB_val key = {.mv_size = 17, .mv_data = (void *)ptr->data};
  MDB_val data;

  // 在 arr 数据库中查找
  int rc = mdb_get(txn->mdb_txn, txn->env->arr_dbi, &key, &data);
  if (rc == MDB_NOTFOUND) {
    return 0; // 不存在
  }

  if (rc == MDB_SUCCESS) {
    return 1; // 存在
  }

  // 其他返回mdb_get
  return rc;
}

// 创建数组
int lmjcore_arr_create(lmjcore_txn *txn, lmjcore_ptr *ptr) {
  if (!txn || !ptr || txn->type != LMJCORE_TXN_WRITE) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  // 生成指针
  int rc = txn->env->ptr_generator(txn->env->ptr_gen_ctx, ptr->data);
  if (rc != LMJCORE_SUCCESS) {
    return rc;
  }

  // 确保指针类型为数组
  ptr->data[0] = LMJCORE_ARR;

  // 在 arr 数据库中创建空成员列表（不存储任何值，仅表示对象存在）
  MDB_val key, data;
  key.mv_data = ptr->data;
  key.mv_size = 17;

  data.mv_data = NULL;
  data.mv_size = 0;

  rc = mdb_put(txn->mdb_txn, txn->env->arr_dbi, &key, &data, 0);
  if (rc != MDB_SUCCESS) {
    return rc;
  }

  return LMJCORE_SUCCESS;
}

// 数组追加元素
int lmjcore_arr_append(lmjcore_txn *txn, const lmjcore_ptr *arr_ptr,
                       const uint8_t *value, size_t value_len) {
  if (!txn || !arr_ptr || !value || txn->type != LMJCORE_TXN_WRITE) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  MDB_val key = {.mv_size = 17, .mv_data = (void *)arr_ptr->data};
  MDB_val mdb_val = {.mv_size = value_len, .mv_data = (void *)value};

  int rc =
      mdb_put(txn->mdb_txn, txn->env->arr_dbi, &key, &mdb_val, MDB_APPENDDUP);
  if (rc != MDB_SUCCESS) {
    return rc;
  }

  return LMJCORE_SUCCESS;
}

/**
 * @brief 统计对象成员的数据大小（从指定锚点成员开始双向扫描）
 * @param txn 事务句柄
 * @param obj_ptr 对象指针
 * @param total_value_len_out 返回所有成员值的总字节数
 * @param total_value_count_out 返回所有成员值的数量
 * @return 错误码
 */
int lmjcore_obj_stat_values(lmjcore_txn *txn, const lmjcore_ptr *obj_ptr,
                            size_t *total_value_len_out,
                            size_t *total_value_count_out) {
  if (!txn || !obj_ptr || !total_value_len_out || !total_value_count_out)
    return LMJCORE_ERROR_INVALID_PARAM;

  *total_value_len_out = 0;
  *total_value_count_out = 0;

  MDB_cursor *cursor = NULL;
  int rc = mdb_cursor_open(txn->mdb_txn, txn->env->main_dbi, &cursor);
  if (rc != MDB_SUCCESS) {
    return rc;
  }

  // 构建搜索key - 对象指针
  MDB_val key = {.mv_data = (void *)obj_ptr->data, .mv_size = 17};
  MDB_val value;

  // 使用MDB_SET_RANGE定位到该对象的第一个成员
  rc = mdb_cursor_get(cursor, &key, &value, MDB_SET_RANGE);
  if (rc == MDB_NOTFOUND) {
    // 对象没有成员，这是正常情况
    rc = LMJCORE_SUCCESS;
    goto cleanup;
  }
  if (rc != MDB_SUCCESS) {
    goto cleanup;
  }

  // 遍历所有属于该对象的成员
  do {
    // 检查当前key是否还属于同一个对象
    if (key.mv_size < 17 || memcmp(key.mv_data, obj_ptr->data, 17) != 0) {
      // 已经移动到下一个对象，结束遍历
      break;
    }

    *total_value_count_out += 1;
    *total_value_len_out += value.mv_size;

    // 移动到下一个key
    rc = mdb_cursor_get(cursor, &key, &value, MDB_NEXT);
  } while (rc == MDB_SUCCESS);

  // MDB_NOTFOUND表示遍历结束，这是正常情况
  if (rc == MDB_NOTFOUND) {
    rc = LMJCORE_SUCCESS;
  }

cleanup:
  if (cursor) {
    mdb_cursor_close(cursor);
  }

  // 如果出现错误，重置统计结果
  if (rc != LMJCORE_SUCCESS) {
    *total_value_count_out = 0;
    *total_value_len_out = 0;
  }

  return rc;
}

/**
 * @brief 统计数组元素长度
 *
 * @param txn 事务句柄
 * @param ptr 实体指针
 * @param total_elment_len_out 输出元素总长度
 * @param element_countout 输出元素计数
 * @return int 错误码
 */
int lmjcore_arr_stat_element(lmjcore_txn *txn, const lmjcore_ptr *ptr,
                             size_t *total_value_len_out,
                             size_t *element_count_out) {
  if (!txn || !ptr || !total_value_len_out || !element_count_out) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  // 初始化
  *total_value_len_out = 0;
  *element_count_out = 0;

  // 开启游标启用
  MDB_cursor *cursor;
  int rc = mdb_cursor_open(txn->mdb_txn, txn->env->arr_dbi, &cursor);
  if (rc != MDB_SUCCESS) {
    mdb_cursor_close(cursor);
    return rc;
  }

  // 开始遍历成员列表
  MDB_val key = {.mv_data = (void *)ptr->data, .mv_size = 17};
  MDB_val value;
  rc = mdb_cursor_get(cursor, &key, &value, MDB_SET);
  if (rc != MDB_SUCCESS) {
    mdb_cursor_close(cursor);
    return rc;
  }
  while (rc != MDB_NOTFOUND) {
    *total_value_len_out += value.mv_size;
    *element_count_out += 1;
    rc = mdb_cursor_get(cursor, &key, &value, MDB_NEXT_DUP);
  }

  mdb_cursor_close(cursor);
  return LMJCORE_SUCCESS;
}

int lmjcore_obj_member_get(lmjcore_txn *txn, const lmjcore_ptr *obj_ptr,
                           const char *member_name, size_t member_name_len,
                           uint8_t *value_buf, size_t value_buf_size,
                           size_t value_buf_offset, size_t *value_size) {
  if (!txn || !obj_ptr || !member_name || !value_size ||
      obj_ptr->data[0] != LMJCORE_OBJ) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  // 确认对象存在
  int rc = lmjcore_exist(txn, obj_ptr);
  if (rc <= 0) {
    return rc == 0 ? LMJCORE_ERROR_ENTITY_NOT_FOUND : rc;
  }

  // 成员名长度检查
  if (member_name_len > MAX_MEMBER_LENGTH) {
    return LMJCORE_ERROR_MEMBER_TOO_LONG;
  }

  // 初始化
  *value_size = 0;

  // 拼接完整的key
  uint8_t *t_key = malloc(17 + member_name_len);
  if (t_key == NULL) {
    return LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED;
  }
  memcpy(t_key, obj_ptr->data, 17);
  memcpy(t_key + 17, member_name, member_name_len);

  // 查询成员的值
  MDB_val key = {.mv_data = t_key, .mv_size = 17 + member_name_len};
  MDB_val value;
  rc = mdb_get(txn->mdb_txn, txn->env->main_dbi, &key, &value);
  free(t_key);
  if (rc != MDB_SUCCESS) {
    return rc; // 注意如果是缺值状态(arr库中存在mian库未找到)会返回MDB_NOTFOUND调用者注意检查以及处理
  }

  // 检查缓冲区是否足够
  if (value_buf_offset + value.mv_size > value_buf_size) {
    return LMJCORE_ERROR_BUFFER_TOO_SMALL;
  }

  // 将值复制到缓冲区
  memcpy(value_buf + value_buf_offset, value.mv_data, value.mv_size);
  *value_size = value.mv_size;

  return LMJCORE_SUCCESS;
}

int lmjcore_obj_get(lmjcore_txn *txn, const lmjcore_ptr *obj_ptr,
                    lmjcore_query_mode mode, uint8_t *result_buf,
                    size_t result_buf_size, lmjcore_result **result_head) {
  if (!txn || !obj_ptr || !result_buf || !result_head ||
      obj_ptr->data[0] != LMJCORE_OBJ) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  // 初始化返回空间
  memset(result_buf, 0, result_buf_size);

  // 设置返回头在缓冲区起始位置
  lmjcore_result *result = (lmjcore_result *)result_buf;
  result->error_count = 0;
  result->count = 0;
  *result_head = result;

  // 最小缓冲区检查
  if (result_buf_size <
      sizeof(lmjcore_result) + sizeof(lmjcore_obj_descriptor)) {
    add_buffer_error(result, obj_ptr);
    return LMJCORE_ERROR_BUFFER_TOO_SMALL;
  }

  // 对象存在性检查
  int rc = lmjcore_exist(txn, obj_ptr);
  if (rc < 0) {
    add_lmdb_error(result, obj_ptr, rc);
    return rc;
  }
  if (rc == 0) {
    add_entity_not_found_error(result, obj_ptr);
    return LMJCORE_ERROR_ENTITY_NOT_FOUND;
  }

  // 内存布局定义：
  // +------------------------+ ← result_buf (result)
  // | lmjcore_result         | 固定头部
  // +------------------------+
  // | member_descriptors[]   | 描述符从前向后增长
  // +------------------------+
  // | ...                    |
  // +------------------------+
  // | name & value data      | 数据从后向前增长
  // +------------------------+ ← result_buf + result_buf_size

  // 计算关键边界指针
  uint8_t *descriptors_start = result_buf + sizeof(lmjcore_result);
  uint8_t *data_end = result_buf + result_buf_size;

  lmjcore_obj_descriptor *current_descriptor =
      (lmjcore_obj_descriptor *)descriptors_start;
  uint8_t *current_data = data_end; // 数据从末尾开始

  // 开启游标读取成员列表
  MDB_cursor *cursor;
  rc = mdb_cursor_open(txn->mdb_txn, txn->env->arr_dbi, &cursor);
  if (rc != MDB_SUCCESS) {
    add_lmdb_error(result, obj_ptr, rc);
    return rc;
  }

  MDB_val key = {.mv_data = (void *)obj_ptr->data, .mv_size = 17};
  MDB_val member_name_val;

  // 定位到该对象的成员列表开始位置
  rc = mdb_cursor_get(cursor, &key, &member_name_val, MDB_SET);
  if (rc == MDB_NOTFOUND) {
    // 空对象 - 没有成员
    mdb_cursor_close(cursor);
    return LMJCORE_SUCCESS;
  }
  if (rc != MDB_SUCCESS) {
    add_lmdb_error(result, obj_ptr, rc);
    mdb_cursor_close(cursor);
    return rc;
  }

  // 单次遍历处理所有成员
  while (rc == MDB_SUCCESS) {
    size_t member_name_len = member_name_val.mv_size;

    // 检查描述符空间是否足够
    uint8_t *next_descriptor = (uint8_t *)(current_descriptor + 1);
    if (next_descriptor >= current_data) {
      add_buffer_error(result, obj_ptr);
      mdb_cursor_close(cursor);
      return LMJCORE_ERROR_BUFFER_TOO_SMALL;
    }

    // 构建完整key查询成员值
    uint8_t *full_key = malloc(17 + member_name_len);
    if (!full_key) {
      add_error_to_result(result, LMJCORE_READERR_MEMORY_ALLOCATION_FAILED,
                          obj_ptr, 0, 0,
                          LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED);
      mdb_cursor_close(cursor);
      return LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED;
    }

    memcpy(full_key, obj_ptr->data, 17);
    memcpy(full_key + 17, member_name_val.mv_data, member_name_len);

    MDB_val member_key = {.mv_data = full_key, .mv_size = 17 + member_name_len};
    MDB_val member_value;

    // 查询成员值
    rc = mdb_get(txn->mdb_txn, txn->env->main_dbi, &member_key, &member_value);

    if (rc != MDB_SUCCESS) {
      // 成员值缺失处理
      current_descriptor->name_len = member_name_len;
      current_descriptor->value_len = 0;
      current_descriptor->value_offset = 0; // 表示null

      // 尝试存储成员名称（如果空间足够）
      if (current_data - member_name_len >= next_descriptor) {
        current_data -= member_name_len;
        memcpy(current_data, member_name_val.mv_data, member_name_len);
        current_descriptor->name_offset = current_data - result_buf;
      } else {
        // 名称空间不足
        current_descriptor->name_offset = 0;
        add_buffer_error(result, obj_ptr);
        return LMJCORE_ERROR_BUFFER_TOO_SMALL;
      }

      add_member_missing_error(result, obj_ptr, current_descriptor->name_offset,
                               current_descriptor->name_len);

      if (mode == LMJCORE_MODE_STRICT) {
        free(full_key);
        mdb_cursor_close(cursor);
        return LMJCORE_ERROR_STRICT_MODE_VIOLATION;
      }
    } else {
      // 成功获取成员值
      size_t total_needed = member_name_len + member_value.mv_size;

      // 检查数据空间是否足够
      if (current_data - total_needed < next_descriptor) {
        free(full_key);
        add_buffer_error(result, obj_ptr);
        mdb_cursor_close(cursor);
        return LMJCORE_ERROR_BUFFER_TOO_SMALL;
      }

      // 存储名称和值
      current_data -= member_value.mv_size;
      memcpy(current_data, member_value.mv_data, member_value.mv_size);
      current_descriptor->value_offset = current_data - result_buf;
      current_descriptor->value_len = member_value.mv_size;

      current_data -= member_name_len;
      memcpy(current_data, member_name_val.mv_data, member_name_len);
      current_descriptor->name_offset = current_data - result_buf;
      current_descriptor->name_len = member_name_len;
    }

    free(full_key);

    // 移动到下一个描述符位置
    current_descriptor++;
    result->count++;

    // 获取下一个成员名称
    rc = mdb_cursor_get(cursor, &key, &member_name_val, MDB_NEXT_DUP);
  }

  mdb_cursor_close(cursor);

  // 设置最终的成员描述符指针
  result->descriptor.object_descriptors =
      (lmjcore_obj_descriptor *)descriptors_start;

  return LMJCORE_SUCCESS;
}

/**
 * @brief 获取数组的所有元素
 *
 * 从指定数组指针读取所有元素，支持宽松和严格模式。
 * - 严格模式：若数组不存在则立即返回错误。
 * - 宽松模式：若数组不存在或为空，返回空数组结构并记录错误。
 *
 * 结果写入调用方提供的 result_buf 缓冲区，采用紧凑布局：
 *  [ lmjcore_result头部 | value_descriptor数组 | 元素数据（从后向前） ]
 *
 * @param txn 有效事务句柄
 * @param arr_ptr 数组指针
 * @param mode 查询模式（宽松/严格）
 * @param result_buf 输出缓冲区
 * @param result_buf_size 缓冲区大小
 * @param result_head 输出：指向 result_buf 中 lmjcore_result 结构的指针
 * @return 错误码（LMJCORE_SUCCESS 表示成功，即使有语义错误也视为成功）
 */
int lmjcore_arr_get(lmjcore_txn *txn, const lmjcore_ptr *arr_ptr,
                    lmjcore_query_mode mode, uint8_t *result_buf,
                    size_t result_buf_size, lmjcore_result **result_head) {
  if (!txn || !arr_ptr || !result_buf || !result_head ||
      result_buf_size < sizeof(lmjcore_result)) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  // 初始化结果结构
  lmjcore_result *result = (lmjcore_result *)result_buf;
  memset(result, 0, sizeof(lmjcore_result));
  *result_head = result;

  // 检查数组是否存在（通过 arr 数据库）
  MDB_val key = {.mv_size = 17, .mv_data = (void *)arr_ptr->data};
  MDB_val data;
  int rc = mdb_get(txn->mdb_txn, txn->env->arr_dbi, &key, &data);

  if (rc == MDB_NOTFOUND) {
    // 数组不存在
    if (mode == LMJCORE_MODE_STRICT) {
      add_entity_not_found_error(result, arr_ptr);
      return LMJCORE_SUCCESS;
    } else {
      // 宽松模式：返回空数组
      result->count = 0;
      result->descriptor.array_descriptors = NULL;
      add_entity_not_found_error(result, arr_ptr);
      return LMJCORE_SUCCESS;
    }
  } else if (rc != MDB_SUCCESS) {
    add_lmdb_error(result, arr_ptr, rc);
    return LMJCORE_SUCCESS;
  }

  // 数组存在，开始遍历其元素
  MDB_cursor *cursor;
  rc = mdb_cursor_open(txn->mdb_txn, txn->env->arr_dbi, &cursor);
  if (rc != MDB_SUCCESS) {
    add_lmdb_error(result, arr_ptr, rc);
    return LMJCORE_SUCCESS;
  }

  // 定位到数组的第一个元素
  rc = mdb_cursor_get(cursor, &key, &data, MDB_SET);
  if (rc != MDB_SUCCESS) {
    mdb_cursor_close(cursor);
    add_lmdb_error(result, arr_ptr, rc);
    return LMJCORE_SUCCESS;
  }

  // 预计算所需空间
  size_t total_data_size = 0;
  size_t element_count = 0;
  MDB_val tmp_key = key, tmp_val = data;

  do {
    total_data_size += tmp_val.mv_size;
    element_count++;
    rc = mdb_cursor_get(cursor, &tmp_key, &tmp_val, MDB_NEXT_DUP);
  } while (rc == MDB_SUCCESS &&
           memcmp(tmp_key.mv_data, arr_ptr->data, 17) == 0);

  // 重置游标到起始位置
  mdb_cursor_get(cursor, &key, &data, MDB_SET);

  // 计算所需总空间
  size_t descriptors_size = element_count * sizeof(lmjcore_arr_descriptor);
  size_t available_data_space =
      result_buf_size - sizeof(lmjcore_result) - descriptors_size;

  if (total_data_size > available_data_space) {
    mdb_cursor_close(cursor);
    add_buffer_error(result, arr_ptr);
    return LMJCORE_SUCCESS;
  }

  // 填充结果结构
  result->count = element_count;
  result->descriptor.array_descriptors =
      (lmjcore_arr_descriptor *)(result_buf + sizeof(lmjcore_result));

  // 数据存储从缓冲区末尾向前写入
  uint8_t *data_write_ptr = result_buf + result_buf_size;
  MDB_cursor *read_cursor;
  mdb_cursor_open(txn->mdb_txn, txn->env->arr_dbi, &read_cursor);
  mdb_cursor_get(read_cursor, &key, &data, MDB_SET);

  for (size_t i = 0; i < element_count; i++) {
    data_write_ptr -= data.mv_size;
    memcpy(data_write_ptr, data.mv_data, data.mv_size);

    result->descriptor.array_descriptors[i].value_offset =
        data_write_ptr - result_buf;
    result->descriptor.array_descriptors[i].value_len = data.mv_size;

    if (i < element_count - 1) {
      mdb_cursor_get(read_cursor, &key, &data, MDB_NEXT_DUP);
    }
  }

  mdb_cursor_close(read_cursor);
  mdb_cursor_close(cursor);

  return LMJCORE_SUCCESS;
}

/*
  report_buf内存结构
  ---------------------  <-- report_buf(report_head)
  |report_head         |
  ---------------------
  |report_descriptor   | <-- 前往后
  ----------------------
  |....                |
  ----------------------
  | member_name        | <-- 由后往前
  ---------------------- <-- data_offset(report_buf_size)
  */
int lmjcore_audit_object(lmjcore_txn *txn, const lmjcore_ptr *obj_ptr,
                         uint8_t *report_buf, size_t report_buf_size,
                         lmjcore_audit_report **report_head) {
  if (!txn || !obj_ptr || !report_buf || !report_head ||
      obj_ptr->data[0] != LMJCORE_OBJ) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  if (report_buf_size < sizeof(lmjcore_audit_report)) {
    return LMJCORE_ERROR_BUFFER_TOO_SMALL;
  }

  // 初始化报告头
  lmjcore_audit_report *report = (lmjcore_audit_report *)report_buf;
  report->audit_cont = 0;
  report->audit_descriptor =
      (lmjcore_audit_descriptor *)(report_buf + sizeof(lmjcore_audit_report));
  *report_head = report;

  size_t report_desc_offset = sizeof(lmjcore_audit_report);
  size_t data_offset = report_buf_size;

  MDB_val_list *registered_members = NULL; // 来自 arr_dbi
  MDB_val_list *ghost_candidates = NULL;   // 来自 main_dbi，用于检测幽灵成员

  int rc = 0;

  // Step 1: 从 arr_dbi 读取所有已注册的成员名
  MDB_cursor *cursor = NULL;
  rc = mdb_cursor_open(txn->mdb_txn, txn->env->arr_dbi, &cursor);
  if (rc != MDB_SUCCESS)
    goto cleanup;

  MDB_val key = {.mv_data = (void *)obj_ptr->data, .mv_size = 17};
  MDB_val value;
  rc = mdb_cursor_get(cursor, &key, &value, MDB_FIRST_DUP);
  while (rc == MDB_SUCCESS) {
    if (!mdb_val_list_append(&registered_members, value)) {
      rc = LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED;
      goto cleanup;
    }
    rc = mdb_cursor_get(cursor, &key, &value, MDB_NEXT_DUP);
  }
  if (rc != MDB_NOTFOUND)
    goto cleanup;
  mdb_cursor_close(cursor);
  cursor = NULL;

  // Step 2: 从 main_dbi 读取所有实际存储的成员（key = obj_ptr + member_name）
  rc = mdb_cursor_open(txn->mdb_txn, txn->env->main_dbi, &cursor);
  if (rc != MDB_SUCCESS)
    goto cleanup;

  // 使用 MDB_SET_RANGE + 手动检查 key 前缀
  rc = mdb_cursor_get(cursor, &key, &value, MDB_SET_RANGE);
  while (rc == MDB_SUCCESS) {
    // 检查 key 是否属于当前 obj_ptr（前17字节匹配）
    if (key.mv_size < 17 || memcmp(key.mv_data, obj_ptr->data, 17) != 0) {
      break; // 已超出当前对象的 key 范围
    }

    // 提取 member_name: key[17:]
    if (key.mv_size > 17) {
      MDB_val member_name = {.mv_data = (uint8_t *)key.mv_data + 17,
                             .mv_size = key.mv_size - 17};

      // 尝试从 registered_members 中移除（表示该成员已注册且有值）
      if (!mdb_val_list_remove(&registered_members, &member_name)) {
        // 未注册 → 幽灵成员
        int err = add_audit(report, report_buf, LMJCORE_AUDITERR_GHOST_MEMBER,
                            &member_name, obj_ptr, &data_offset,
                            &report_desc_offset, report_buf_size);
        if (err != LMJCORE_SUCCESS) {
          rc = err;
          goto cleanup;
        }
      }
    }

    rc = mdb_cursor_get(cursor, &key, &value, MDB_NEXT);
  }

  if (rc != MDB_NOTFOUND && rc != MDB_SUCCESS)
    goto cleanup;

  // Step 3: registered_members 中剩下的就是“缺失值”（已注册但无值）
  MDB_val_list *node = registered_members;
  while (node) {
    int err = add_audit(report, report_buf, LMJCORE_AUDITERR_MISSING_VALUE,
                        &node->val, obj_ptr, &data_offset, &report_desc_offset,
                        report_buf_size);
    if (err != LMJCORE_SUCCESS) {
      rc = err;
      goto cleanup;
    }
    node = node->next;
  }

  rc = LMJCORE_SUCCESS;

cleanup:
  if (cursor)
    mdb_cursor_close(cursor);
  mdb_val_list_free_all(registered_members);
  mdb_val_list_free_all(ghost_candidates);
  return rc;
}

int lmjcore_repair_object(lmjcore_txn *txn, uint8_t *report_buf,
                          size_t report_buf_size,
                          lmjcore_audit_report *report) {
  if (!txn || !report || !report_buf) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  // 没有错误
  if (report->audit_descriptor == NULL || report->audit_cont == 0) {
    return LMJCORE_SUCCESS;
  }

  int final_result = LMJCORE_SUCCESS;

  // 遍历整个错误列表
  for (size_t i = 0; i < report->audit_cont; i++) {
    lmjcore_audit_descriptor *desc = &report->audit_descriptor[i];

    if (desc->error == LMJCORE_AUDITERR_GHOST_MEMBER) {
      // 验证成员名偏移和长度
      if (desc->member_offset + desc->member_len > report_buf_size) {
        final_result = LMJCORE_ERROR_INVALID_PARAM;
        break;
      }

      // 构建幽灵成员的完整key: <17字节指针> + <成员名>
      size_t key_len = 17 + desc->member_len;
      uint8_t *ghost_key = malloc(key_len);
      if (!ghost_key) {
        final_result = LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED;
        break;
      }

      memcpy(ghost_key, desc->ptr.data, 17);
      memcpy(ghost_key + 17, report_buf + desc->member_offset,
             desc->member_len);

      MDB_val key = {.mv_data = ghost_key, .mv_size = key_len};
      MDB_val value;

      int error = mdb_del(txn->mdb_txn, txn->env->main_dbi, &key, &value);
      free(ghost_key);

      if (error != MDB_SUCCESS) {
        final_result = error;
        break; // 遇到第一个错误就停止，保持事务一致性
      }
    }
  }

  return final_result;
}