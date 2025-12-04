# LMJCore Toolkit CONFIG Object
## 配置对象 (Config Object)

**本质：** 配置对象是一个类型为 `LMJCORE_OBJ` 的**普通对象**。这意味着它在 `main` 库和 `arr` 库中的存储方式与任何其他对象完全相同。

**特殊性：** 它的特殊性仅源于其在系统中的地位和生命周期管理：
1. **固定指针：** 配置对象使用固定全零指针 `0x0000000000000000000000000000000000` (17字节)
2. **初始化创建：** 它由用户初始化和连接，支持在初始化 lmjcore 后任意时间初始化和连接
3. **系统级语义：** 作为系统配置的根入口点，承载全局配置信息

---

## 一、设计原则

### 1.1 兼容性优先
- **不引入新API**：完全复用现有的 `lmjcore_obj_*` 系列函数
- **不修改内核**：配置对象管理作为工具层实现，保持内核纯净
- **语义化约定**：通过固定指针和命名约定实现特殊语义

### 1.2 生命周期管理
- **惰性初始化**：首次访问时自动创建（如不存在）
- **持久化存储**：与普通对象一样持久化在 LMDB 中
- **事务安全**：遵循 LMJCore 标准事务规则

---

## 二、技术实现

### 2.1 固定指针定义

```c
// 类型前缀为对象，其余为0
static const uint8_t LMJCORE_CONFIG_OBJECT_PTR[LMJCORE_PTR_LEN] = {
    LMJCORE_OBJ,  // 类型前缀
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
```

### 2.2 工具函数（头文件声明）

```c
// config_object.h
#ifndef LMJCORE_CONFIG_OBJECT_H
#define LMJCORE_CONFIG_OBJECT_H

#include "lmjcore.h"

#ifdef __cplusplus
extern "C" {
#endif

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
```
---

## 三、设计优势

### 3.1 内核兼容性
- ✅ 不修改 LMJCore 内核代码
- ✅ 完全复用现有对象操作 API
- ✅ 遵循现有事务和内存管理规范

### 3.2 使用便捷性
- ✅ 固定指针，无需管理配置对象生命周期
- ✅ 惰性初始化，首次访问自动创建
- ✅ 语义清晰的工具函数封装

### 3.3 系统一致性
- ✅ 配置数据与其他数据统一存储管理
- ✅ 享受 LMJCore 完整的 ACID 事务保障
- ✅ 支持审计、修复等所有内核工具函数

---

## 四、注意事项

1. **指针唯一性**：全零指针在系统中唯一，避免与其他对象冲突
2. **初始化时机**：配置对象在首次 `lmjcore_config_set` 或显式调用 `ensure` 时创建
3. **性能影响**：额外的存在性检查对性能影响可忽略不计
4. **存储开销**：与普通对象存储开销完全相同

这个设计在保持 LMJCore 内核纯净的同时，通过工具层提供了便捷的配置对象管理功能，完全符合您的"不定义新读取写入接口"的要求。