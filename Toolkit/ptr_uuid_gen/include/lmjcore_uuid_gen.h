// lmjcore_uuid_gen.h
#ifndef LMJCORE_UUID_GEN_H
#define LMJCORE_UUID_GEN_H

#include "../../../core/include/lmjcore.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UUIDv4 指针生成器上下文结构
 *
 * 用于指定要生成的实体类型（OBJ/ARR）
 */
typedef struct {
    lmjcore_entity_type type; // 必须是 LMJCORE_OBJ 或 LMJCORE_ARR
} lmjcore_uuid_gen_ctx;

/**
 * @brief UUIDv4 指针生成器函数（符合 lmjcore_ptr_generator_fn 签名）
 *
 * @param ctx 必须指向 lmjcore_uuid_gen_ctx 结构
 * @param out 输出缓冲区（17 字节），out[0] = type, out[1..16] = UUIDv4
 * @return int
 *   - LMJCORE_SUCCESS: 成功
 *   - LMJCORE_ERROR_INVALID_PARAM: ctx 无效或类型非法
 *   - LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED: 随机数生成失败（如 /dev/urandom 打不开）
 */
int lmjcore_uuidv4_ptr_gen(void *ctx, uint8_t out[LMJCORE_PTR_LEN]);

#ifdef __cplusplus
}
#endif

#endif // LMJCORE_UUID_GEN_H