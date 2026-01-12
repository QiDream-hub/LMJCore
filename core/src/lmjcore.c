#include "../include/lmjcore.h"
#include <lmdb.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#define MAIN_DB_NAME "main"
#define ARR_DB_NAME "arr"

/*
 *==========================================
 * 内部结构
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

/*
 *==========================================
 * 内部工具函数
 *==========================================
 */

/**
 * @brief 检查MDB_val的key是否以指定的对象指针开头
 * @param obj_ptr 对象指针
 * @param key LMDB的key值
 * @return true表示属于该对象，false表示不属于
 */
static inline bool OBJ_KEY_PREFIX(const lmjcore_ptr obj_ptr, MDB_val key) {
  return (key.mv_size >= LMJCORE_PTR_LEN) &&
         (memcmp(key.mv_data, obj_ptr, LMJCORE_PTR_LEN) == 0);
}

/**

 * @brief 向对象结果中添加错误

 */
static bool result_obj_add_error(lmjcore_result_obj *result,
                                 lmjcore_read_error_code code,
                                 size_t element_offset, size_t element_len,
                                 const lmjcore_ptr entity_ptr) {
  if (!result || !entity_ptr) {
    return false;
  }
  if (result->error_count >= LMJCORE_MAX_READ_ERRORS) {
    return true;
  }
  lmjcore_read_error *err = &result->errors[result->error_count];
  err->code = code;
  err->element.element_offset = element_offset;
  err->element.element_len = element_len;
  memcpy(err->entity_ptr, entity_ptr, LMJCORE_PTR_LEN);
  result->error_count++;
  return true;
}

/**
 * @brief 向数组结果中添加错误
 */
static bool result_arr_add_error(lmjcore_result_arr *result,
                                 lmjcore_read_error_code code,
                                 size_t element_offset, size_t element_len,
                                 const lmjcore_ptr entity_ptr) {
  if (!result || !entity_ptr) {
    return false;
  }
  if (result->error_count >= LMJCORE_MAX_READ_ERRORS) {
    return false;
  }
  lmjcore_read_error *err = &result->errors[result->error_count];
  err->code = code;
  err->element.element_offset = element_offset;
  err->element.element_len = element_len;
  memcpy(err->entity_ptr, entity_ptr, LMJCORE_PTR_LEN);
  result->error_count++;
  return true;
}

/**
 * @brief 添加审计记录
 */
static int add_audit(uint8_t *buf, const MDB_val *member_name,
                     const MDB_val *member_value, const lmjcore_ptr obj_ptr,
                     size_t *data_offset, size_t *report_descriptor_offset) {
  if (!buf || !member_name || !member_value || !obj_ptr || !data_offset ||
      !report_descriptor_offset) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  size_t needed = member_name->mv_size + member_value->mv_size;
  size_t desc_size = sizeof(lmjcore_audit_descriptor);

  // 检查描述符区是否有空间
  if (*report_descriptor_offset + desc_size > *data_offset) {
    return LMJCORE_ERROR_BUFFER_TOO_SMALL;
  }

  // 检查数据区是否有空间（从后往前写）
  if (*data_offset < needed) {
    return LMJCORE_ERROR_BUFFER_TOO_SMALL;
  }

  // 初始化审计描述符
  lmjcore_audit_descriptor descriptor = {0};

  // 写入成员名（从缓冲区末尾向前）
  *data_offset -= member_name->mv_size;
  memcpy(buf + *data_offset, member_name->mv_data, member_name->mv_size);
  // 设置成员名描述符
  descriptor.member.member_name.value_offset = *data_offset;
  descriptor.member.member_name.value_len = member_name->mv_size;

  // 写入成员值
  *data_offset -= member_value->mv_size;
  memcpy(buf + *data_offset, member_value->mv_data, member_value->mv_size);

  descriptor.member.member_value.value_offset = *data_offset;
  descriptor.member.member_value.value_len = member_value->mv_size;
  // 设置指针
  memcpy(descriptor.ptr, obj_ptr, LMJCORE_PTR_LEN);

  // 将描述符写入buff
  memcpy(buf + *report_descriptor_offset, &descriptor,
         sizeof(lmjcore_audit_descriptor));

  *report_descriptor_offset += desc_size;

  return LMJCORE_SUCCESS;
}

/**
 * @brief 从arr库读取指定指针的所有关联值（通用实现）
 *
 * 不区分对象成员或数组元素，只是读取arr[ptr]的所有values
 */
static int arr_get_all_values(lmjcore_txn *txn, const lmjcore_ptr ptr,
                              uint8_t *result_buf, size_t result_buf_size,
                              lmjcore_result_arr **result_head) {
  if (!txn || !ptr || !result_buf || !result_head) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  // 最小缓存空间检查
  if (result_buf_size <
      (sizeof(lmjcore_result_arr) + sizeof(lmjcore_descriptor))) {
    return LMJCORE_ERROR_BUFFER_TOO_SMALL;
  }

  memset(result_buf, 0, result_buf_size);

  // 初始化结果结构
  lmjcore_result_arr *result = (lmjcore_result_arr *)result_buf;
  result->element_count = 0;
  result->error_count = 0;
  *result_head = result;

  // 计算内存分区边界
  size_t descriptor_offset = sizeof(lmjcore_result_arr); // 当前描述符写入位置
  size_t next_descriptor_offset = descriptor_offset;     // 下一个描述符开始位置
  size_t data_offset = result_buf_size;                  // 当前数据写入位置

  // 数组存在，开始遍历其元素
  MDB_val key = {.mv_size = LMJCORE_PTR_LEN, .mv_data = (void *)ptr};
  MDB_val data;
  MDB_cursor *cursor;
  int rc = mdb_cursor_open(txn->mdb_txn, txn->env->arr_dbi, &cursor);
  if (rc != MDB_SUCCESS) {
    return rc; // lmdb错误
  }

  // 定位到数组的第一个元素
  rc = mdb_cursor_get(cursor, &key, &data, MDB_SET);
  if (rc == MDB_NOTFOUND) {
    mdb_cursor_close(cursor);
    result->element_count = 0;
    result->error_count += 1;
    result_arr_add_error(result, LMJCORE_READERR_ENTITY_NOT_FOUND, 0, 0, ptr);
    return LMJCORE_SUCCESS; // 有意设计：空数组返回成功
  }
  if (rc != MDB_SUCCESS) {
    mdb_cursor_close(cursor);
    return rc;
  }

  while (rc == MDB_SUCCESS) {
    // 检查是否有足够空间存放数据
    if (data.mv_size > data_offset) {
      mdb_cursor_close(cursor);
      return LMJCORE_ERROR_BUFFER_TOO_SMALL;
    }

    // 检查是否有足够空间存放描述符
    size_t needed_for_descriptor =
        next_descriptor_offset + sizeof(lmjcore_descriptor);
    if (needed_for_descriptor > data_offset) {
      mdb_cursor_close(cursor);
      return LMJCORE_ERROR_BUFFER_TOO_SMALL;
    }

    // 检查是否有足够空间同时存放数据和描述符
    if (needed_for_descriptor > (data_offset - data.mv_size)) {
      mdb_cursor_close(cursor);
      return LMJCORE_ERROR_BUFFER_TOO_SMALL;
    }

    // 预留数据空间（从后往前）
    data_offset -= data.mv_size;

    // 向buff中填写数据
    memcpy(result_buf + data_offset, data.mv_data, data.mv_size);

    // 填写描述符
    lmjcore_descriptor desc;
    desc.value_offset = data_offset;
    desc.value_len = data.mv_size;
    memcpy(result_buf + descriptor_offset, &desc, sizeof(lmjcore_descriptor));

    // 更新偏移量
    descriptor_offset += sizeof(lmjcore_descriptor);
    next_descriptor_offset = descriptor_offset;
    result->element_count += 1;

    rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT_DUP);
  }

  mdb_cursor_close(cursor);

  if (rc != MDB_NOTFOUND) {
    return rc;
  }

  return LMJCORE_SUCCESS;
}

/**
 * @brief 统计arr库中指定指针的所有值的总长度和数量（通用实现）
 */
static int arr_stat_values(lmjcore_txn *txn, const lmjcore_ptr ptr,
                           size_t *total_size_out, size_t *count_out) {
  if (!txn || !ptr || !total_size_out || !count_out) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  // 初始化
  *total_size_out = 0;
  *count_out = 0;

  // 开启游标启用
  MDB_cursor *cursor;
  int rc = mdb_cursor_open(txn->mdb_txn, txn->env->arr_dbi, &cursor);
  if (rc != MDB_SUCCESS) {
    mdb_cursor_close(cursor);
    return rc;
  }

  // 开始遍历成员列表
  MDB_val key = {.mv_data = (void *)ptr, .mv_size = LMJCORE_PTR_LEN};
  MDB_val value;
  rc = mdb_cursor_get(cursor, &key, &value, MDB_SET);
  if (rc != MDB_SUCCESS) {
    mdb_cursor_close(cursor);
    return rc;
  }
  while (rc != MDB_NOTFOUND) {
    *total_size_out += value.mv_size;
    *count_out += 1;
    rc = mdb_cursor_get(cursor, &key, &value, MDB_NEXT_DUP);
  }

  mdb_cursor_close(cursor);
  return LMJCORE_SUCCESS;
}

/*
 *==========================================
 * 初始化环境与清理
 *==========================================
 */
// 初始化lmjcore环境
int lmjcore_init(const char *path, size_t map_size, unsigned int flags,
                 lmjcore_ptr_generator_fn ptr_gen, void *ptr_gen_ctx,
                 lmjcore_env **env) {
  if (!path || !env || !ptr_gen) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  lmjcore_env *new_env = calloc(1, sizeof(lmjcore_env));
  if (!new_env) {
    return LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED;
  }

  // 设置指针生成器
  new_env->ptr_generator = ptr_gen;
  new_env->ptr_gen_ctx = ptr_gen_ctx;

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
  rc = mdb_env_open(new_env->mdb_env, path, flags, 0664);
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

/*
 *==========================================
 * 事务相关
 *==========================================
 */
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

/*
 *==========================================
 * 对象相关
 ==========================================
 */
// 创建对象
int lmjcore_obj_create(lmjcore_txn *txn, lmjcore_ptr ptr_out) {
  if (!txn || !ptr_out || txn->type != LMJCORE_TXN_WRITE) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  // 生成指针
  int rc = txn->env->ptr_generator(txn->env->ptr_gen_ctx, ptr_out);
  if (rc != LMJCORE_SUCCESS) {
    return rc;
  }

  // 确保指针类型为对象
  ptr_out[0] = LMJCORE_OBJ;

  // 在 arr 数据库中创建空成员列表（不存储任何值，仅表示对象存在）
  MDB_val key, data;
  key.mv_data = ptr_out;
  key.mv_size = LMJCORE_PTR_LEN;

  data.mv_data = NULL;
  data.mv_size = 0;

  rc = mdb_put(txn->mdb_txn, txn->env->arr_dbi, &key, &data, 0);
  if (rc != MDB_SUCCESS) {
    return rc;
  }
  return LMJCORE_SUCCESS;
}

// 注册对象
int lmjcore_obj_register(lmjcore_txn *txn, const lmjcore_ptr ptr) {
  if (!txn || !ptr || txn->type != LMJCORE_TXN_WRITE || ptr[0] != LMJCORE_OBJ ||
      lmjcore_entity_exist(txn, ptr) == 1) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }
  // 在 arr 数据库中创建空成员列表（不存储任何值，仅表示对象存在）
  MDB_val key, data;
  key.mv_data = (void *)ptr;
  key.mv_size = LMJCORE_PTR_LEN;

  data.mv_data = NULL;
  data.mv_size = 0;

  int rc = mdb_put(txn->mdb_txn, txn->env->arr_dbi, &key, &data, 0);
  if (rc != MDB_SUCCESS) {
    return rc;
  }
  return LMJCORE_SUCCESS;
}

// 读取对象
int lmjcore_obj_get(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                    uint8_t *result_buf, size_t result_buf_size,
                    lmjcore_result_obj **result_head) {
  if (!txn || !obj_ptr || !result_buf || !result_head ||
      obj_ptr[0] != LMJCORE_OBJ) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  // 初始化返回空间
  memset(result_buf, 0, result_buf_size);

  // 设置返回头在缓冲区起始位置
  lmjcore_result_obj *result = (lmjcore_result_obj *)result_buf;
  result->error_count = 0;
  result->member_count = 0;

  // 计算关键边界指针
  uint8_t *descriptors_start = result_buf + sizeof(lmjcore_result_obj);

  *result_head = result;

  // 最小缓冲区检查
  if (result_buf_size <
      sizeof(lmjcore_result_obj) + sizeof(lmjcore_member_descriptor)) {
    return LMJCORE_ERROR_BUFFER_TOO_SMALL;
  }

  // 对象存在性检查
  int rc = lmjcore_entity_exist(txn, obj_ptr);
  if (rc < 0) {
    return rc; // lmdb 错误
  }
  if (rc == 0) {
    // 实体不存在
    result_obj_add_error(result, LMJCORE_READERR_ENTITY_NOT_FOUND, 0, 0,
                         obj_ptr);
    return LMJCORE_SUCCESS;
  }

  // 内存布局定义：
  // +------------------------+ ← result_buf (result)
  // | lmjcore_result_obj     | 固定头部
  // +------------------------+
  // | member_descriptors[]   | 描述符从前向后增长
  // +------------------------+
  // | ...                    |
  // +------------------------+
  // | name & value data      | 数据从后向前增长
  // +------------------------+ ← result_buf + result_buf_size

  uint8_t *data_end = result_buf + result_buf_size;

  lmjcore_member_descriptor *current_descriptor =
      (lmjcore_member_descriptor *)descriptors_start;
  uint8_t *current_data = data_end; // 数据从末尾开始

  // 开启游标读取成员列表
  MDB_cursor *cursor;
  rc = mdb_cursor_open(txn->mdb_txn, txn->env->arr_dbi, &cursor);
  if (rc != MDB_SUCCESS) {
    return rc; // lmdb错误直接返回
  }

  MDB_val key = {.mv_data = (void *)obj_ptr, .mv_size = LMJCORE_PTR_LEN};
  MDB_val member_name_val;

  // 定位到该对象的成员列表开始位置
  rc = mdb_cursor_get(cursor, &key, &member_name_val, MDB_SET);
  if (rc == MDB_NOTFOUND) {
    // 空对象 - 没有成员
    mdb_cursor_close(cursor);
    *result_head = result;
    return LMJCORE_SUCCESS;
  }
  if (rc != MDB_SUCCESS) {
    mdb_cursor_close(cursor);
    return rc; // lmdb错误直接返回
  }

  // 单次遍历处理所有成员
  while (rc == MDB_SUCCESS) {
    size_t member_name_len = member_name_val.mv_size;

    // 检查描述符空间是否足够
    uint8_t *next_descriptor = (uint8_t *)(current_descriptor + 1);
    if (next_descriptor >= current_data) {
      mdb_cursor_close(cursor);
      return LMJCORE_ERROR_BUFFER_TOO_SMALL; // 缓存太小直接返回(致命错误)
    }

    // 构建完整key查询成员值
    uint8_t full_key[LMJCORE_MAX_KEY_LEN];
    memcpy(full_key, obj_ptr, LMJCORE_PTR_LEN);
    memcpy(full_key + LMJCORE_PTR_LEN, member_name_val.mv_data,
           member_name_len);

    MDB_val member_key = {.mv_data = full_key,
                          .mv_size = LMJCORE_PTR_LEN + member_name_len};
    MDB_val member_value;

    // 查询成员值
    rc = mdb_get(txn->mdb_txn, txn->env->main_dbi, &member_key, &member_value);

    if (rc != MDB_SUCCESS) {
      // 成员值缺失处理
      current_descriptor->member_name.value_len = member_name_len;
      current_descriptor->member_value.value_len = 0;
      current_descriptor->member_value.value_offset = 0; // 表示null

      // 尝试存储成员名称（如果空间足够）
      if (current_data - member_name_len >= next_descriptor) {
        current_data -= member_name_len;
        memcpy(current_data, member_name_val.mv_data, member_name_len);
        current_descriptor->member_name.value_offset =
            current_data - result_buf;
      } else {
        // 名称空间不足
        current_descriptor->member_name.value_offset = 0;
        mdb_cursor_close(cursor);
        return LMJCORE_ERROR_BUFFER_TOO_SMALL; // 致命错误缓存太小
      }

      result_obj_add_error(result, LMJCORE_READERR_MEMBER_MISSING,
                           current_descriptor->member_name.value_offset,
                           current_descriptor->member_name.value_len, obj_ptr);
    } else {
      // 成功获取成员值
      size_t total_needed = member_name_len + member_value.mv_size;

      // 检查数据空间是否足够
      if (current_data - total_needed < next_descriptor) {
        mdb_cursor_close(cursor);
        return LMJCORE_ERROR_BUFFER_TOO_SMALL; // 致命错误缓存空间不够
      }

      // 存储名称和值
      current_data -= member_value.mv_size;
      memcpy(current_data, member_value.mv_data, member_value.mv_size);
      current_descriptor->member_value.value_offset = current_data - result_buf;
      current_descriptor->member_value.value_len = member_value.mv_size;

      current_data -= member_name_len;
      memcpy(current_data, member_name_val.mv_data, member_name_len);
      current_descriptor->member_name.value_offset = current_data - result_buf;
      current_descriptor->member_name.value_len = member_name_len;
    }

    // 移动到下一个描述符位置
    current_descriptor++;
    result->member_count++;

    // 获取下一个成员名称
    rc = mdb_cursor_get(cursor, &key, &member_name_val, MDB_NEXT_DUP);
  }

  mdb_cursor_close(cursor);

  // 设置返回头
  *result_head = result;

  return LMJCORE_SUCCESS;
}

// 删除对象
int lmjcore_obj_del(lmjcore_txn *txn, const lmjcore_ptr obj_ptr) {
  if (!txn || !obj_ptr || txn->type != LMJCORE_TXN_WRITE ||
      obj_ptr[0] != LMJCORE_OBJ) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  MDB_cursor *cursor = NULL;
  int rc = mdb_cursor_open(txn->mdb_txn, txn->env->arr_dbi, &cursor);
  if (rc != MDB_SUCCESS) {
    return rc;
  }

  // 构建对象key
  MDB_val key = {.mv_data = (void *)obj_ptr, .mv_size = LMJCORE_PTR_LEN};
  MDB_val value;

  rc = mdb_cursor_get(cursor, &key, &value, MDB_NEXT_DUP);
  if (rc == MDB_NOTFOUND) {
    // 没有成员，直接删除 arr 条目
    rc = mdb_del(txn->mdb_txn, txn->env->arr_dbi, &key, NULL);
    mdb_cursor_close(cursor);
    return (rc == MDB_SUCCESS) ? LMJCORE_SUCCESS : rc;
  }

  if (rc == MDB_SUCCESS) {
    // 遍历所有成员，删除值
    while (rc == MDB_SUCCESS) {
      rc = lmjcore_obj_member_value_del(txn, obj_ptr, value.mv_data,
                                        value.mv_size);
      if (rc != LMJCORE_SUCCESS) {
        break;
      }
      rc = mdb_cursor_get(cursor, &key, &value, MDB_NEXT_DUP);
    }
  }

  // 无论成功还是失败，都清理游标
  mdb_cursor_close(cursor);

  // 最后删除 arr 条目
  if (rc == MDB_NOTFOUND || rc == MDB_SUCCESS) {
    MDB_val del_key = {.mv_data = (void *)obj_ptr, .mv_size = LMJCORE_PTR_LEN};
    rc = mdb_del(txn->mdb_txn, txn->env->arr_dbi, &del_key, NULL);
  }

  return (rc == MDB_SUCCESS || rc == MDB_NOTFOUND) ? LMJCORE_SUCCESS : rc;
}
/*
 *==========================================
 * 对象成员相关
 *==========================================
 */
// 对象成员写入
int lmjcore_obj_member_put(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                           const uint8_t *member_name, size_t member_name_len,
                           const uint8_t *value, size_t value_len) {
  if (!txn || !obj_ptr || !member_name || txn->type != LMJCORE_TXN_WRITE ||
      obj_ptr[0] != LMJCORE_OBJ ||
      member_name_len > LMJCORE_MAX_MEMBER_NAME_LEN) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  // 检查成员名长度
  if (member_name_len > LMJCORE_MAX_MEMBER_NAME_LEN) {
    return LMJCORE_ERROR_MEMBER_TOO_LONG;
  }

  // 在 arr 数据库中注册成员名（如果尚未注册）
  MDB_val arr_key = {.mv_size = LMJCORE_PTR_LEN, .mv_data = (void *)obj_ptr};
  MDB_val arr_val = {.mv_size = member_name_len,
                     .mv_data = (void *)member_name};

  // 使用lmdb数据库的原生检查插入
  int rc = mdb_put(txn->mdb_txn, txn->env->arr_dbi, &arr_key, &arr_val,
                   MDB_NODUPDATA);
  // 有重复的值
  if (rc == MDB_KEYEXIST || rc == MDB_SUCCESS) {
    // 构建主键：对象指针 + 成员名
    size_t key_size = LMJCORE_PTR_LEN + member_name_len;
    uint8_t key[key_size];

    memcpy(key, obj_ptr, LMJCORE_PTR_LEN);
    memcpy(key + LMJCORE_PTR_LEN, member_name, member_name_len);

    // 写入 main 数据库
    MDB_val mdb_key = {.mv_size = key_size, .mv_data = key};
    MDB_val mdb_val = {.mv_size = value_len, .mv_data = (void *)value};

    rc = mdb_put(txn->mdb_txn, txn->env->main_dbi, &mdb_key, &mdb_val, 0);

    if (rc != MDB_SUCCESS) {
      return rc;
    }
  }

  return rc;
}

// 注册成员
int lmjcore_obj_member_register(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                                const uint8_t *member_name,
                                size_t member_name_len) {
  if (!txn || !obj_ptr || !member_name || txn->type != LMJCORE_TXN_WRITE ||
      obj_ptr[0] != LMJCORE_OBJ ||
      member_name_len > LMJCORE_MAX_MEMBER_NAME_LEN) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  MDB_val key = {.mv_data = (void *)obj_ptr, .mv_size = LMJCORE_PTR_LEN};
  MDB_val value = {.mv_data = (void *)member_name, .mv_size = member_name_len};
  int rc =
      mdb_put(txn->mdb_txn, txn->env->arr_dbi, &key, &value, MDB_NODUPDATA);
  if (rc != MDB_SUCCESS) {
    return rc;
  }
  return LMJCORE_SUCCESS;
}

// 删除成员值
int lmjcore_obj_member_value_del(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                                 const uint8_t *member_name,
                                 size_t member_name_len) {
  if (!txn || !obj_ptr || !member_name || obj_ptr[0] != LMJCORE_OBJ ||
      member_name_len > LMJCORE_MAX_MEMBER_NAME_LEN) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  uint8_t member_key[LMJCORE_PTR_LEN + member_name_len];
  memcpy(member_key, obj_ptr, LMJCORE_PTR_LEN);
  memcpy(member_key + LMJCORE_PTR_LEN, member_name, member_name_len);
  MDB_val key = {.mv_data = member_key,
                 .mv_size = LMJCORE_PTR_LEN + member_name_len};
  int rc = mdb_del(txn->mdb_txn, txn->env->main_dbi, &key, NULL);
  if (rc != MDB_SUCCESS) {
    return rc;
  }
  return LMJCORE_SUCCESS;
}

int lmjcore_obj_member_get(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                           const uint8_t *member_name, size_t member_name_len,
                           uint8_t *value_buf, size_t value_buf_size,
                           size_t *value_size_out) {
  if (!txn || !obj_ptr || !member_name || !value_size_out ||
      obj_ptr[0] != LMJCORE_OBJ) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  // 确认对象存在
  int rc = lmjcore_entity_exist(txn, obj_ptr);
  if (rc <= 0) {
    return rc == 0 ? LMJCORE_ERROR_ENTITY_NOT_FOUND : rc;
  }

  // 成员名长度检查
  if (member_name_len > LMJCORE_MAX_MEMBER_NAME_LEN) {
    return LMJCORE_ERROR_MEMBER_TOO_LONG;
  }

  // 初始化
  *value_size_out = 0;

  // 拼接完整的key
  uint8_t t_key[LMJCORE_PTR_LEN + member_name_len];
  memcpy(t_key, obj_ptr, LMJCORE_PTR_LEN);
  memcpy(t_key + LMJCORE_PTR_LEN, member_name, member_name_len);

  // 查询成员的值
  MDB_val key = {.mv_data = t_key,
                 .mv_size = LMJCORE_PTR_LEN + member_name_len};
  MDB_val value;
  rc = mdb_get(txn->mdb_txn, txn->env->main_dbi, &key, &value);

  // 转换 LMDB 错误码为 LMJCore 错误码
  if (rc == MDB_NOTFOUND) {
    return LMJCORE_ERROR_MEMBER_NOT_FOUND;
  }

  if (rc != MDB_SUCCESS) {
    return rc; // 其他错误
  }

  // 检查缓冲区是否足够
  if (value.mv_size > value_buf_size) {
    return LMJCORE_ERROR_BUFFER_TOO_SMALL;
  }

  // 将值复制到缓冲区
  memcpy(value_buf, value.mv_data, value.mv_size);
  *value_size_out = value.mv_size;

  return LMJCORE_SUCCESS;
}

// 移除注册(在arr库中删除)
int lmjcore_obj_member_del(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                           const uint8_t *member_name, size_t member_name_len) {
  if (!txn || !obj_ptr || !member_name || obj_ptr[0] != LMJCORE_OBJ ||
      member_name_len > LMJCORE_MAX_MEMBER_NAME_LEN) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  MDB_val key = {.mv_data = (void *)obj_ptr, .mv_size = LMJCORE_PTR_LEN};
  MDB_val value = {.mv_data = (void *)member_name, .mv_size = member_name_len};
  int rc = mdb_del(txn->mdb_txn, txn->env->arr_dbi, &key, &value);
  if (rc != MDB_SUCCESS) {
    return rc;
  }
  return rc;
}

// 统计对象成员名长度
int lmjcore_obj_stat_members(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                             size_t *total_member_len_out,
                             size_t *member_count_out) {
  if (obj_ptr[0] != LMJCORE_OBJ) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }
  return arr_stat_values(txn, obj_ptr, total_member_len_out, member_count_out);
}

// 获取对象成员列表
int lmjcore_obj_member_list(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                            uint8_t *result_buf, size_t result_buf_size,
                            lmjcore_result_arr **result_head) {
  if (obj_ptr[0] != LMJCORE_OBJ) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }
  return arr_get_all_values(txn, obj_ptr, result_buf, result_buf_size,
                            result_head);
}

/*
 *==========================================
 * 数组相关
 *==========================================
 */
// 创建数组
int lmjcore_arr_create(lmjcore_txn *txn, lmjcore_ptr ptr_out) {
  if (!txn || !ptr_out || txn->type != LMJCORE_TXN_WRITE) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  // 生成指针
  int rc = txn->env->ptr_generator(txn->env->ptr_gen_ctx, ptr_out);
  if (rc != LMJCORE_SUCCESS) {
    return rc;
  }

  // 确保指针类型为数组
  ptr_out[0] = LMJCORE_ARR;

  // 在 arr 数据库中创建空列表（不存储任何值，仅表示对象存在）
  MDB_val key, data;
  key.mv_data = ptr_out;
  key.mv_size = LMJCORE_PTR_LEN;

  data.mv_data = NULL;
  data.mv_size = 0;

  rc = mdb_put(txn->mdb_txn, txn->env->arr_dbi, &key, &data, 0);
  if (rc != MDB_SUCCESS) {
    return rc;
  }

  return LMJCORE_SUCCESS;
}

// 数组追加元素
int lmjcore_arr_append(lmjcore_txn *txn, const lmjcore_ptr arr_ptr,
                       const uint8_t *value, size_t value_len) {
  if (!txn || !arr_ptr || !value || txn->type != LMJCORE_TXN_WRITE ||
      arr_ptr[0] != LMJCORE_ARR) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  MDB_val key = {.mv_size = LMJCORE_PTR_LEN, .mv_data = (void *)arr_ptr};
  MDB_val mdb_val = {.mv_size = value_len, .mv_data = (void *)value};

  int rc = mdb_put(txn->mdb_txn, txn->env->arr_dbi, &key, &mdb_val, 0);
  if (rc != MDB_SUCCESS) {
    return rc;
  }

  return LMJCORE_SUCCESS;
}

// 删除这个数组
int lmjcore_arr_del(lmjcore_txn *txn, const lmjcore_ptr arr_ptr) {
  if (!txn || !arr_ptr || txn->type != LMJCORE_TXN_WRITE ||
      arr_ptr[0] != LMJCORE_ARR) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }
  MDB_val key = {.mv_data = (void *)arr_ptr, .mv_size = LMJCORE_PTR_LEN};
  int rc = mdb_del(txn->mdb_txn, txn->env->arr_dbi, &key, NULL);
  if (rc != MDB_SUCCESS) {
    return rc;
  }
  return LMJCORE_SUCCESS;
}

// 删除数组中的元素
int lmjcore_arr_element_del(lmjcore_txn *txn, const lmjcore_ptr arr_ptr,
                            const uint8_t *element, size_t element_len) {
  if (!txn || !arr_ptr || !element || txn->type != LMJCORE_TXN_WRITE ||
      arr_ptr[0] != LMJCORE_ARR) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }
  MDB_val key = {.mv_data = (void *)arr_ptr, .mv_size = LMJCORE_PTR_LEN};
  MDB_val vale = {.mv_data = (void *)element, .mv_size = element_len};
  int rc = mdb_del(txn->mdb_txn, txn->env->arr_dbi, &key, &vale);
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
int lmjcore_obj_stat_values(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
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
  MDB_val key = {.mv_data = (void *)obj_ptr, .mv_size = LMJCORE_PTR_LEN};
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
    if (key.mv_size < LMJCORE_PTR_LEN ||
        memcmp(key.mv_data, obj_ptr, LMJCORE_PTR_LEN) != 0) {
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

// 读取数组的所有元素
int lmjcore_arr_get(lmjcore_txn *txn, const lmjcore_ptr arr_ptr,
                    uint8_t *result_buf, size_t result_buf_size,
                    lmjcore_result_arr **result_head) {
  if (arr_ptr[0] != LMJCORE_ARR) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }
  return arr_get_all_values(txn, arr_ptr, result_buf, result_buf_size,
                            result_head);
}

/**
 * @brief 统计数组元素长度
 */
int lmjcore_arr_stat_element(lmjcore_txn *txn, const lmjcore_ptr arr_ptr,
                             size_t *total_value_len_out,
                             size_t *element_count_out) {
  if (arr_ptr[0] != LMJCORE_ARR) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }
  return arr_stat_values(txn, arr_ptr, total_value_len_out, element_count_out);
}

/*
 *==========================================
 * 审计与修复
 *==========================================
 */
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
int lmjcore_audit_object(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                         uint8_t *report_buf, size_t report_buf_size,
                         lmjcore_audit_report **report_head) {
  if (!txn || !obj_ptr || !report_buf || !report_head ||
      obj_ptr[0] != LMJCORE_OBJ) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  if (report_buf_size < sizeof(lmjcore_audit_report)) {
    return LMJCORE_ERROR_BUFFER_TOO_SMALL;
  }

  // 初始化报告头
  lmjcore_audit_report *report = (lmjcore_audit_report *)report_buf;
  report->audit_cont = 0;
  *report_head = report;

  size_t report_desc_offset = sizeof(lmjcore_audit_report); // 描述符数组偏移量
  size_t data_offset = report_buf_size;                     // 数据偏移量

  MDB_cursor *cursor_main, *cursor_arr;
  int rc = mdb_cursor_open(txn->mdb_txn, txn->env->main_dbi,
                           &cursor_main); // 开启main库游标
  if (rc != MDB_SUCCESS) {
    return rc;
  }
  rc = mdb_cursor_open(txn->mdb_txn, txn->env->arr_dbi,
                       &cursor_arr); // 开启arr库游标
  if (rc != MDB_SUCCESS) {
    return rc;
  }

  MDB_val obj = {.mv_data = (void *)obj_ptr, .mv_size = LMJCORE_PTR_LEN};
  MDB_val key = obj; // 第一个成员使用前缀匹配
  MDB_val value;
  rc = mdb_cursor_get(cursor_main, &key, &value, MDB_SET_RANGE);
  while (rc == MDB_SUCCESS) {
    // 判断是否处于目标对象中
    if (memcmp(obj_ptr, key.mv_data, LMJCORE_PTR_LEN) != 0) {
      rc = LMJCORE_SUCCESS;
      break;
    }
    // 判断成员名是否在arr库中注册
    MDB_val member_name = {.mv_data = key.mv_data + LMJCORE_PTR_LEN,
                           .mv_size = key.mv_size - LMJCORE_PTR_LEN};
    int exist = mdb_cursor_get(cursor_arr, &obj, &member_name, MDB_GET_BOTH);
    // 查询arr库如果找到会返回MDB_SUCCESS未找到会返回MDB_NOTFOUND(幽灵成员)
    if (exist == MDB_NOTFOUND) {
      int error = add_audit(report_buf, &member_name, &value, obj_ptr,
                            &data_offset, &report_desc_offset);
      if (error != LMJCORE_SUCCESS) {
        rc = error;
        goto cleanup;
      }
      // 记录审计数量
      report->audit_cont++;
    } else if (exist != MDB_SUCCESS) {
      rc = exist;
      goto cleanup;
    }
    rc = mdb_cursor_get(cursor_main, &key, &value, MDB_NEXT);
  }
  if (rc == MDB_NOTFOUND) {
    rc = LMJCORE_SUCCESS;
  }
cleanup:
  if (cursor_arr) {
    mdb_cursor_close(cursor_arr);
  }
  if (cursor_main) {
    mdb_cursor_close(cursor_main);
  }
  return rc;
}

int lmjcore_repair_object(lmjcore_txn *txn, uint8_t *report_buf,
                          size_t report_buf_size,
                          lmjcore_audit_report *report) {
  if (!txn || !report || !report_buf) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  // 没有错误
  if (report->audit_cont == 0) {
    return LMJCORE_SUCCESS;
  }

  int final_result = LMJCORE_SUCCESS;

  // 遍历整个错误列表
  for (size_t i = 0; i < report->audit_cont; i++) {
    lmjcore_audit_descriptor *desc = &report->audit_descriptor[i];

    // 验证成员名偏移和长度
    if (desc->member.member_name.value_offset +
            desc->member.member_name.value_len >
        report_buf_size) {
      final_result = LMJCORE_ERROR_INVALID_PARAM;
      break;
    }

    // 成员值的验证
    if (desc->member.member_value.value_offset +
            desc->member.member_value.value_len >
        report_buf_size) {
      final_result = LMJCORE_ERROR_INVALID_PARAM;
      break;
    }

    // 构建幽灵成员的完整key: <LMJCORE_PTR_LEN字节指针> + <成员名>
    size_t key_len = LMJCORE_PTR_LEN + desc->member.member_name.value_len;
    uint8_t ghost_key[key_len];

    memcpy(ghost_key, desc->ptr, LMJCORE_PTR_LEN);
    memcpy(ghost_key + LMJCORE_PTR_LEN,
           report_buf + desc->member.member_name.value_offset,
           desc->member.member_name.value_len);

    MDB_val key = {.mv_data = ghost_key, .mv_size = key_len};
    MDB_val value;

    int error = mdb_del(txn->mdb_txn, txn->env->main_dbi, &key, &value);

    if (error != MDB_SUCCESS) {
      final_result = error;
      break; // 遇到第一个错误就停止，保持事务一致性
    }
  }

  return final_result;
}

/*
 *==========================================
 * 存在性检查
 *==========================================
 */
// 实体存在性检查
int lmjcore_entity_exist(lmjcore_txn *txn, const lmjcore_ptr ptr) {
  if (!txn || !ptr) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  MDB_val key = {.mv_size = LMJCORE_PTR_LEN, .mv_data = (void *)ptr};
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

// 成员值存在性检查
int lmjcore_obj_member_value_exist(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                                   const uint8_t *member_name,
                                   size_t member_name_len) {
  if (!txn || !obj_ptr || !member_name || txn->type != LMJCORE_TXN_READONLY ||
      obj_ptr[0] != LMJCORE_OBJ ||
      member_name_len > LMJCORE_MAX_MEMBER_NAME_LEN) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  // 申请临时内存,用于存储成员key
  uint8_t member_key[LMJCORE_PTR_LEN + member_name_len];
  memcpy(member_key, obj_ptr, LMJCORE_PTR_LEN);
  memcpy(member_key + LMJCORE_PTR_LEN, member_name, member_name_len);
  MDB_val key = {.mv_data = (void *)member_key,
                 .mv_size = LMJCORE_PTR_LEN + member_name_len};
  MDB_val value;

  int rc = mdb_get(txn->mdb_txn, txn->env->main_dbi, &key, &value);
  if (rc == MDB_SUCCESS) {
    return 1;
  }
  if (rc == MDB_NOTFOUND) {
    return 0;
  }
  return rc;
}

/*
 *==========================================
 * 工具函数
 *==========================================
 */
// 指针转字符串
int lmjcore_ptr_to_string(const lmjcore_ptr ptr, char *str_buf,
                          size_t buf_size) {
  if (!ptr || !str_buf || buf_size < LMJCORE_PTR_STRING_LEN) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  const char hex_chars[] = "0123456789abcdef";
  for (int i = 0; i < LMJCORE_PTR_LEN; i++) {
    uint8_t byte = ptr[i];
    str_buf[i * 2] = hex_chars[(byte >> 4) & 0x0F];
    str_buf[i * 2 + 1] = hex_chars[byte & 0x0F];
  }
  str_buf[LMJCORE_PTR_STRING_LEN] = '\0';

  return LMJCORE_SUCCESS;
}

// 字符串转指针
int lmjcore_ptr_from_string(const char *str, lmjcore_ptr ptr_out) {
  if (!str || !ptr_out || strlen(str) != LMJCORE_PTR_STRING_LEN) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  for (int i = 0; i < LMJCORE_PTR_LEN; i++) {
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

    ptr_out[i] = (high << 4) | low;
  }

  return LMJCORE_SUCCESS;
}

// 判断事务是否符合对应类型
bool is_txn_type(lmjcore_txn *txn, lmjcore_txn_type type) {
  return txn->type == type;
}