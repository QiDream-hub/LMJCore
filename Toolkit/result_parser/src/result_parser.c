#include "../include/result_parser.h"
#include <string.h>

// ==================== 对象结果解析 ====================

int lmjcore_parser_obj_find_member(const lmjcore_result_obj *result,
                                   const uint8_t *result_buf,
                                   const uint8_t *member_name,
                                   size_t member_name_len,
                                   const uint8_t **value_data,
                                   size_t *value_len) {
    if (!result || !result_buf || !member_name || member_name_len == 0) {
        return LMJCORE_ERROR_INVALID_PARAM;
    }

    for (size_t i = 0; i < result->member_count; ++i) {
        const lmjcore_member_descriptor *desc = &result->members[i];
        const uint8_t *name_ptr = result_buf + desc->member_name.value_offset;
        if (desc->member_name.value_len == member_name_len &&
            memcmp(name_ptr, member_name, member_name_len) == 0) {
            if (value_data) *value_data = result_buf + desc->member_value.value_offset;
            if (value_len) *value_len = desc->member_value.value_len;
            return LMJCORE_SUCCESS;
        }
    }

    return LMJCORE_ERROR_ENTITY_NOT_FOUND;
}

int lmjcore_parser_obj_get_member(const lmjcore_result_obj *result,
                                  const uint8_t *result_buf, size_t index,
                                  const uint8_t **name_data, size_t *name_len,
                                  const uint8_t **value_data,
                                  size_t *value_len) {
    if (!result || !result_buf || index >= result->member_count) {
        return LMJCORE_ERROR_ENTITY_NOT_FOUND;
    }

    const lmjcore_member_descriptor *desc = &result->members[index];
    if (name_data) *name_data = result_buf + desc->member_name.value_offset;
    if (name_len) *name_len = desc->member_name.value_len;
    if (value_data) *value_data = result_buf + desc->member_value.value_offset;
    if (value_len) *value_len = desc->member_value.value_len;

    return LMJCORE_SUCCESS;
}

size_t lmjcore_parser_obj_member_count(const lmjcore_result_obj *result) {
    return result ? result->member_count : 0;
}

// ==================== 数组结果解析 ====================

int lmjcore_parser_arr_get_element(const lmjcore_result_arr *result,
                                   const uint8_t *result_buf, size_t index,
                                   const uint8_t **element_data,
                                   size_t *element_len) {
    if (!result || !result_buf || index >= result->element_count) {
        return LMJCORE_ERROR_ENTITY_NOT_FOUND;
    }

    const lmjcore_descriptor *desc = &result->elements[index];
    if (element_data) *element_data = result_buf + desc->value_offset;
    if (element_len) *element_len = desc->value_len;

    return LMJCORE_SUCCESS;
}

int lmjcore_parser_arr_find_element(const lmjcore_result_arr *result,
                                    const uint8_t *result_buf,
                                    const uint8_t *element, size_t element_len,
                                    size_t *found_index) {
    if (!result || !result_buf || !element || element_len == 0) {
        return LMJCORE_ERROR_INVALID_PARAM;
    }

    for (size_t i = 0; i < result->element_count; ++i) {
        const lmjcore_descriptor *desc = &result->elements[i];
        if (desc->value_len == element_len &&
            memcmp(result_buf + desc->value_offset, element, element_len) == 0) {
            if (found_index) *found_index = i;
            return LMJCORE_SUCCESS;
        }
    }

    return LMJCORE_ERROR_ENTITY_NOT_FOUND;
}

size_t lmjcore_parser_arr_element_count(const lmjcore_result_arr *result) {
    return result ? result->element_count : 0;
}

// ==================== 错误信息解析 ====================

size_t lmjcore_parser_error_count(const void *result) {
    if (!result) return 0;

    // 判断是 obj 还是 arr（通过检查前两个字段是否合理）
    const lmjcore_result_obj *obj = (const lmjcore_result_obj *)result;
    // 假设 error_count 是第一个字段（对齐一致）
    return obj->error_count;
}

int lmjcore_parser_get_error(const void *result, size_t index,
                             const lmjcore_read_error **error) {
    if (!result || !error || index >= LMJCORE_MAX_READ_ERRORS) {
        return LMJCORE_ERROR_ENTITY_NOT_FOUND;
    }

    const lmjcore_result_obj *obj = (const lmjcore_result_obj *)result;
    if (index >= obj->error_count) {
        return LMJCORE_ERROR_ENTITY_NOT_FOUND;
    }

    *error = &obj->errors[index];
    return LMJCORE_SUCCESS;
}

bool lmjcore_parser_has_error(const void *result,
                              lmjcore_read_error_code error_code) {
    if (!result) return false;

    const lmjcore_result_obj *obj = (const lmjcore_result_obj *)result;
    for (size_t i = 0; i < obj->error_count; ++i) {
        if (obj->errors[i].code == error_code) {
            return true;
        }
    }
    return false;
}