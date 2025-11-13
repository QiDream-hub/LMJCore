// result_parser.c
#include "../include/result_parser.h"
#include <string.h>

// ==================== 对象结果解析 ====================

int lmjcore_parser_obj_find_member(const lmjcore_result *result, const uint8_t *result_buf,
                                  const uint8_t *member_name, size_t member_name_len,
                                  const uint8_t **value_data, size_t *value_len)
{
    if (!result || !result_buf || !member_name || !value_data || !value_len) {
        return LMJCORE_ERROR_INVALID_PARAM;
    }

    if (result->count == 0) {
        return LMJCORE_ERROR_ENTITY_NOT_FOUND;
    }

    // 遍历所有成员描述符
    for (size_t i = 0; i < result->count; i++) {
        const lmjcore_obj_descriptor *desc = &result->descriptor.object_descriptors[i];
        
        // 检查成员名长度是否匹配
        if (desc->name_len != member_name_len) {
            continue;
        }
        
        // 获取成员名数据指针
        const uint8_t *current_name = result_buf + desc->name_offset;
        
        // 比较成员名内容
        if (memcmp(current_name, member_name, member_name_len) == 0) {
            // 找到匹配的成员
            *value_data = result_buf + desc->value_offset;
            *value_len = desc->value_len;
            return LMJCORE_SUCCESS;
        }
    }
    
    return LMJCORE_ERROR_ENTITY_NOT_FOUND;
}

int lmjcore_parser_obj_get_member(const lmjcore_result *result, const uint8_t *result_buf,
                                 size_t index,
                                 const uint8_t **name_data, size_t *name_len,
                                 const uint8_t **value_data, size_t *value_len)
{
    if (!result || !result_buf || !name_data || !name_len || !value_data || !value_len) {
        return LMJCORE_ERROR_INVALID_PARAM;
    }

    if (index >= result->count) {
        return LMJCORE_ERROR_ENTITY_NOT_FOUND;
    }

    const lmjcore_obj_descriptor *desc = &result->descriptor.object_descriptors[index];
    
    *name_data = result_buf + desc->name_offset;
    *name_len = desc->name_len;
    *value_data = result_buf + desc->value_offset;
    *value_len = desc->value_len;
    
    return LMJCORE_SUCCESS;
}

size_t lmjcore_parser_obj_member_count(const lmjcore_result *result)
{
    if (!result) {
        return 0;
    }
    return result->count;
}

// ==================== 数组结果解析 ====================

int lmjcore_parser_arr_get_element(const lmjcore_result *result, const uint8_t *result_buf,
                                  size_t index,
                                  const uint8_t **element_data, size_t *element_len)
{
    if (!result || !result_buf || !element_data || !element_len) {
        return LMJCORE_ERROR_INVALID_PARAM;
    }

    if (index >= result->count) {
        return LMJCORE_ERROR_ENTITY_NOT_FOUND;
    }

    const lmjcore_arr_descriptor *desc = &result->descriptor.array_descriptors[index];
    
    *element_data = result_buf + desc->value_offset;
    *element_len = desc->value_len;
    
    return LMJCORE_SUCCESS;
}

int lmjcore_parser_arr_find_element(const lmjcore_result *result, const uint8_t *result_buf,
                                   const uint8_t *element, size_t element_len,
                                   size_t *found_index)
{
    if (!result || !result_buf || !element || !found_index) {
        return LMJCORE_ERROR_INVALID_PARAM;
    }

    if (result->count == 0) {
        return LMJCORE_ERROR_ENTITY_NOT_FOUND;
    }

    // 遍历所有元素描述符
    for (size_t i = 0; i < result->count; i++) {
        const lmjcore_arr_descriptor *desc = &result->descriptor.array_descriptors[i];
        
        // 检查元素长度是否匹配
        if (desc->value_len != element_len) {
            continue;
        }
        
        // 获取元素数据指针
        const uint8_t *current_element = result_buf + desc->value_offset;
        
        // 比较元素内容
        if (memcmp(current_element, element, element_len) == 0) {
            *found_index = i;
            return LMJCORE_SUCCESS;
        }
    }
    
    return LMJCORE_ERROR_ENTITY_NOT_FOUND;
}

size_t lmjcore_parser_arr_element_count(const lmjcore_result *result)
{
    if (!result) {
        return 0;
    }
    return result->count;
}

// ==================== 错误信息解析 ====================

size_t lmjcore_parser_error_count(const lmjcore_result *result)
{
    if (!result) {
        return 0;
    }
    return result->error_count;
}

int lmjcore_parser_get_error(const lmjcore_result *result, size_t index,
                            const lmjcore_read_error **error)
{
    if (!result || !error) {
        return LMJCORE_ERROR_INVALID_PARAM;
    }

    if (index >= result->error_count) {
        return LMJCORE_ERROR_ENTITY_NOT_FOUND;
    }

    *error = &result->errors[index];
    return LMJCORE_SUCCESS;
}

bool lmjcore_parser_has_error(const lmjcore_result *result, lmjcore_read_error_code error_code)
{
    if (!result) {
        return false;
    }

    for (size_t i = 0; i < result->error_count; i++) {
        if (result->errors[i].code == error_code) {
            return true;
        }
    }
    
    return false;
}