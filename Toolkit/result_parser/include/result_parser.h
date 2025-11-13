// result_parser.h
#ifndef LMJCORE_RESULT_PARSER_H
#define LMJCORE_RESULT_PARSER_H

#include "../../../core/include/lmjcore.h"

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