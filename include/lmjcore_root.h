// lmjcore_root.h
#ifndef LMJCORE_ROOT_H
#define LMJCORE_ROOT_H

#include "lmjcore.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Root 对象特定的错误码
#define LMJCORE_ROOT_ERROR_ALREADY_INITIALIZED -33000 // Root 对象已初始化
#define LMJCORE_ROOT_ERROR_DIFFERENT_ROOT                                      \
  -33001 // 尝试使用不同的根指针重新初始化
#define LMJCORE_ROOT_ERROR_NOT_INITIALIZED -33002  // Root 对象未初始化
#define LMJCORE_ROOT_ERROR_INVALID_ROOT_PTR -33003 // 无效的根指针格式

// 全 0 指针作为固定 config 对象指针
static uint8_t CONFIG_OBJECT_PTR[17] = {
    LMJCORE_OBJ, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00,        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/**
 * @brief 初始化根对象
 *
 * 遵循单次初始化原则：
 * - 如果根对象不存在，创建新的根对象
 * - 如果根对象已存在，验证指针是否匹配
 * - 相同的根指针视为无操作
 * - 不同的根指针视为严重错误
 *
 * @param txn 写事务
 * @param root_ptr 期望的根对象指针（可为NULL，使用自动生成）
 * @return int 错误码
 */
int lmjcore_root_init(lmjcore_txn *txn, const lmjcore_ptr *root_ptr);

/**
 * @brief 获取当前根对象指针
 *
 * 从系统固定位置读取已注册的根对象指针
 *
 * @param txn 事务（读或写事务均可）
 * @param root_ptr_out 输出的根对象指针
 * @return int 错误码
 */
int lmjcore_get_root_pointer(lmjcore_txn *txn, lmjcore_ptr *root_ptr_out);

/**
 * @brief 检查根对象系统是否已初始化
 *
 * @param txn 事务（读或写事务均可）
 * @return true 已初始化
 * @return false 未初始化
 */
bool lmjcore_root_is_initialized(lmjcore_txn *txn);

/**
 * @brief 强制设置根对象指针（仅用于特殊恢复场景）
 *
 * 警告：此函数会覆盖现有的根对象指针，只能在数据恢复等特殊场景下使用
 *
 * @param txn 写事务
 * @param root_ptr 新的根对象指针
 * @return int 错误码
 */
int lmjcore_root_force_set(lmjcore_txn *txn, const lmjcore_ptr *root_ptr);

#ifdef __cplusplus
}
#endif

#endif // LMJCORE_ROOT_H