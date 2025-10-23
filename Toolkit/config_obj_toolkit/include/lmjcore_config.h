// config_object.h
#ifndef LMJCORE_CONFIG_OBJECT_H
#define LMJCORE_CONFIG_OBJECT_H

#include "../../../core/include/lmjcore.h"

#ifdef __cplusplus
extern "C" {
#endif

// 错误码定义
#define LMJCORE_ERROR_MEMBER_NOT_FOUND -33000

// 类型前缀为对象，其余为0
static const uint8_t LMJCORE_CONFIG_OBJECT_PTR[LMJCORE_PTR_LEN] = {
    LMJCORE_OBJ,  // 类型前缀
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/**
 * @brief 获取配置对象指针（常量引用）
 * 
 * @return const lmjcore_ptr* 配置对象的固定指针
 */
const lmjcore_ptr* lmjcore_config_object_ptr();

/**
 * @brief 确保配置对象存在（惰性初始化）
 * 
 * 如果配置对象不存在，则创建空的配置对象。
 * 此函数应在访问配置对象前调用以确保可用性。
 * 
 * @param txn 写事务
 * @return int 
 *   - LMJCORE_SUCCESS: 配置对象已存在或创建成功
 *   - 其他错误码: 创建失败
 */
int lmjcore_config_object_ensure(lmjcore_txn *txn);

/**
 * @brief 检查配置对象是否存在
 * 
 * @param txn 读事务
 * @return int 
 *   - 1: 配置对象存在
 *   - 0: 配置对象不存在
 *   - <0: 检查过程中发生错误
 */
int lmjcore_config_object_exist(lmjcore_txn *txn);

/**
 * @brief 设置配置项
 * 
 * 等同于 lmjcore_obj_member_put，但自动确保配置对象存在
 * 
 * @param txn 写事务
 * @param key 配置项名称
 * @param key_len 名称长度
 * @param value 配置值
 * @param value_len 值长度
 * @return int 
 */
int lmjcore_config_set(lmjcore_txn *txn, 
                      const uint8_t *key, size_t key_len,
                      const uint8_t *value, size_t value_len);

/**
 * @brief 获取配置项
 * 
 * 等同于 lmjcore_obj_member_get，但使用配置对象指针
 * 
 * @param txn 读事务
 * @param key 配置项名称
 * @param key_len 名称长度
 * @param value_buf 值缓冲区
 * @param value_buf_size 缓冲区大小
 * @param value_size 实际值大小
 * @return int 
 */
int lmjcore_config_get(lmjcore_txn *txn,
                      const uint8_t *key, size_t key_len,
                      uint8_t *value_buf, size_t value_buf_size,
                      size_t *value_size);

/**
 * @brief 获取完整配置对象
 * 
 * 等同于 lmjcore_obj_get，但使用配置对象指针
 * 
 * @param txn 读事务
 * @param mode 读取模式
 * @param result_buf 结果缓冲区
 * @param result_buf_size 缓冲区大小
 * @param result_head 结果头指针
 * @return int 
 */
int lmjcore_config_get_all(lmjcore_txn *txn, lmjcore_query_mode mode,
                          uint8_t *result_buf, size_t result_buf_size,
                          lmjcore_result **result_head);

#ifdef __cplusplus
}
#endif

#endif // LMJCORE_CONFIG_OBJECT_H