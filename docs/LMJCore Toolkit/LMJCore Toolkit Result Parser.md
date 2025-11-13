# LMJCore Toolkit Result Parser

## 结果解析器 (Result Parser)

**本质：** 一组类型明确的工具函数，调用者需指定要解析的是对象还是数组。

**核心原则：** 调用者对自己的数据结构负责，解析器只提供安全的二进制数据访问。

---

## 一、设计原则

### 1.1 显式类型指定
- **调用者负责**：调用者明确知道自己在处理对象还是数组
- **无自动判断**：不尝试猜测结果类型，避免错误解析
- **类型安全**：通过函数命名明确区分对象和数组操作

### 1.2 极简实用
- **核心功能**：只提供最常用的查找和遍历功能
- **零抽象**：直接映射到内存布局，无额外开销
- **边界安全**：所有访问都进行边界检查

---

## 二、技术实现

```c
// result_parser.h
#ifndef LMJCORE_RESULT_PARSER_H
#define LMJCORE_RESULT_PARSER_H

#include "lmjcore.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 对象结果解析 ====================

/**
 * @brief 在对象结果中查找成员值
 * 
 * @param result 结果结构体指针
 * @param result_buf 结果缓冲区
 * @param member_name 成员名称（二进制安全）
 * @param member_name_len 成员名称长度
 * @param value_data 输出参数，值数据指针
 * @param value_len 输出参数，值数据长度
 * @return int 
 *   - LMJCORE_SUCCESS: 找到成员
 *   - LMJCORE_ERROR_ENTITY_NOT_FOUND: 成员不存在
 *   - LMJCORE_ERROR_INVALID_PARAM: 参数无效
 */
int lmjcore_parser_obj_find_member(const lmjcore_result *result, const uint8_t *result_buf,
                                  const uint8_t *member_name, size_t member_name_len,
                                  const uint8_t **value_data, size_t *value_len);

/**
 * @brief 按索引获取对象成员
 * 
 * @param result 结果结构体指针
 * @param result_buf 结果缓冲区
 * @param index 成员索引（0-based）
 * @param name_data 输出参数，名称数据指针
 * @param name_len 输出参数，名称数据长度
 * @param value_data 输出参数，值数据指针
 * @param value_len 输出参数，值数据长度
 * @return int 
 *   - LMJCORE_SUCCESS: 获取成功
 *   - LMJCORE_ERROR_ENTITY_NOT_FOUND: 索引越界
 */
int lmjcore_parser_obj_get_member(const lmjcore_result *result, const uint8_t *result_buf,
                                 size_t index,
                                 const uint8_t **name_data, size_t *name_len,
                                 const uint8_t **value_data, size_t *value_len);

/**
 * @brief 获取对象成员数量
 * 
 * @param result 结果结构体指针
 * @return size_t 成员数量
 */
size_t lmjcore_parser_obj_member_count(const lmjcore_result *result);

// ==================== 数组结果解析 ====================

/**
 * @brief 按索引获取数组元素
 * 
 * @param result 结果结构体指针
 * @param result_buf 结果缓冲区
 * @param index 元素索引（0-based）
 * @param element_data 输出参数，元素数据指针
 * @param element_len 输出参数，元素数据长度
 * @return int 
 *   - LMJCORE_SUCCESS: 获取成功
 *   - LMJCORE_ERROR_ENTITY_NOT_FOUND: 索引越界
 */
int lmjcore_parser_arr_get_element(const lmjcore_result *result, const uint8_t *result_buf,
                                  size_t index,
                                  const uint8_t **element_data, size_t *element_len);

/**
 * @brief 在数组中查找元素（精确字节匹配）
 * 
 * @param result 结果结构体指针
 * @param result_buf 结果缓冲区
 * @param element 要查找的元素数据
 * @param element_len 元素数据长度
 * @param found_index 输出参数，找到的索引位置
 * @return int 
 *   - LMJCORE_SUCCESS: 找到元素
 *   - LMJCORE_ERROR_ENTITY_NOT_FOUND: 元素不存在
 */
int lmjcore_parser_arr_find_element(const lmjcore_result *result, const uint8_t *result_buf,
                                   const uint8_t *element, size_t element_len,
                                   size_t *found_index);

/**
 * @brief 获取数组元素数量
 * 
 * @param result 结果结构体指针
 * @return size_t 元素数量
 */
size_t lmjcore_parser_arr_element_count(const lmjcore_result *result);

// ==================== 错误信息解析 ====================

/**
 * @brief 获取错误数量
 * 
 * @param result 结果结构体指针
 * @return size_t 错误数量
 */
size_t lmjcore_parser_error_count(const lmjcore_result *result);

/**
 * @brief 按索引获取错误信息
 * 
 * @param result 结果结构体指针
 * @param index 错误索引（0-based）
 * @param error 输出参数，错误信息指针
 * @return int 
 *   - LMJCORE_SUCCESS: 获取成功
 *   - LMJCORE_ERROR_ENTITY_NOT_FOUND: 索引越界
 */
int lmjcore_parser_get_error(const lmjcore_result *result, size_t index,
                            const lmjcore_read_error **error);

/**
 * @brief 检查是否包含特定类型的错误
 * 
 * @param result 结果结构体指针
 * @param error_code 错误代码
 * @return bool 是否包含该错误
 */
bool lmjcore_parser_has_error(const lmjcore_result *result, lmjcore_read_error_code error_code);

#ifdef __cplusplus
}
#endif

#endif // LMJCORE_RESULT_PARSER_H
```

---

## 三、使用示例

### 3.1 对象解析（调用者知道这是对象）

```c
uint8_t result_buf[4096];
lmjcore_result *result;

// 获取对象数据 - 调用者知道这是对象
int ret = lmjcore_obj_get(txn, obj_ptr, LMJCORE_MODE_LOOSE, 
                         result_buf, sizeof(result_buf), &result);

// 查找特定成员
const uint8_t *value_data;
size_t value_len;
const uint8_t member_name[] = "username";

ret = lmjcore_parser_obj_find_member(result, result_buf, 
                                    member_name, sizeof(member_name)-1,
                                    &value_data, &value_len);

if (ret == LMJCORE_SUCCESS) {
    // 使用二进制数据 - 调用者负责解释内容
    printf("用户名长度: %zu\n", value_len);
}

// 遍历所有成员
size_t count = lmjcore_parser_obj_member_count(result);
for (size_t i = 0; i < count; i++) {
    const uint8_t *name_data, *value_data;
    size_t name_len, value_len;
    
    lmjcore_parser_obj_get_member(result, result_buf, i,
                                 &name_data, &name_len,
                                 &value_data, &value_len);
    
    printf("成员 %.*s: 值长度=%zu\n", 
           (int)name_len, name_data, value_len);
}
```

### 3.2 数组解析（调用者知道这是数组）

```c
uint8_t result_buf[4096];
lmjcore_result *result;

// 获取数组数据 - 调用者知道这是数组
int ret = lmjcore_arr_get(txn, arr_ptr, LMJCORE_MODE_LOOSE,
                         result_buf, sizeof(result_buf), &result);

// 遍历数组元素
size_t count = lmjcore_parser_arr_element_count(result);
for (size_t i = 0; i < count; i++) {
    const uint8_t *element_data;
    size_t element_len;
    
    lmjcore_parser_arr_get_element(result, result_buf, i,
                                  &element_data, &element_len);
    
    printf("元素[%zu]: 长度=%zu\n", i, element_len);
    
    // 调用者知道元素的具体类型并负责解析
    if (element_len == LMJCORE_PTR_LEN) {
        // 可能是指针
        lmjcore_ptr_to_string((lmjcore_ptr)element_data, str_buf, sizeof(str_buf));
        printf("  可能是指针: %s\n", str_buf);
    }
}
```

### 3.3 错误处理

```c
// 检查错误信息
size_t error_count = lmjcore_parser_error_count(result);
if (error_count > 0) {
    printf("发现 %zu 个错误:\n", error_count);
    
    for (size_t i = 0; i < error_count; i++) {
        const lmjcore_read_error *error;
        if (lmjcore_parser_get_error(result, i, &error) == LMJCORE_SUCCESS) {
            printf("  错误[%zu]: code=%d\n", i, error->code);
        }
    }
}

// 检查特定错误
if (lmjcore_parser_has_error(result, LMJCORE_READERR_MEMBER_MISSING)) {
    printf("严格模式下发现缺失成员\n");
}
```

---

## 四、设计优势

### 4.1 简单直接
- ✅ **无魔法**：调用者明确指定类型，无自动判断
- ✅ **函数明确**：通过命名清晰区分对象和数组操作
- ✅ **零学习成本**：API 设计直观易懂

### 4.2 类型安全
- ✅ **编译时检查**：错误的类型使用会在编译时发现
- ✅ **运行时验证**：所有访问都进行边界检查
- ✅ **明确职责**：调用者负责数据解释，解析器负责安全访问

### 4.3 性能最优
- ✅ **直接内存访问**：基于偏移量直接计算，无额外开销
- ✅ **最小函数集**：只提供最必要的功能
- ✅ **内联友好**：简单函数适合编译器优化