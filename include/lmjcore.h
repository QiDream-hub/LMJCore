#ifndef LMJCORE_H
#define LMJCORE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 错误码定义
#define LMJCORE_SUCCESS 0
#define LMJCORE_ERROR_INVALID_PARAM -32000 // 错误的参数
#define LMJCORE_ERROR_MEMBER_TOO_LONG -32001  // 成员名太长
#define LMJCORE_ERROR_ENTITY_NOT_FOUND -32002  // 实体为找到(无效的指针)
#define LMJCORE_ERROR_GHOST_MEMBER -32003  // 幽灵成员
#define LMJCORE_ERROR_INVALID_POINTER -32004  // 错误的指针(指针格式不对)
#define LMJCORE_ERROR_BUFFER_TOO_SMALL -32005  // 缓冲区太小
#define LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED -32006 // 内存申请错误
#define LMJCORE_ERROR_STRICT_MODE_VIOLATION -32007 // 严格模式读取错误

// 固定大小的错误报告
#define LMJCORE_MAX_READ_ERRORS 8

// 实体类型枚举
typedef enum {
    LMJCORE_OBJ = 0x01,  // 对象
    LMJCORE_ARR = 0x02,  // 数组
} lmjcore_entity_type;

// 查询模式
typedef enum {
    LMJCORE_MODE_LOOSE = 0,  // 宽松模式
    LMJCORE_MODE_STRICT = 1, // 严格模式
} lmjcore_query_mode;

// 事务类型
typedef enum {
    LMJCORE_TXN_READONLY = 0,
    LMJCORE_TXN_WRITE = 1,
} lmjcore_txn_type;

// 读取错误代码定义
typedef enum {
    LMJCORE_READERR_NONE = 0,
    LMJCORE_READERR_ENTITY_NOT_FOUND = 1,   // 实体不存在
    LMJCORE_READERR_MEMBER_MISSING,         // 成员值缺失  
    LMJCORE_READERR_BUFFER_TOO_SMALL,        // 缓冲区不足
    LMJCORE_READERR_LMDB_FAILED,            // LMDB操作失败
    LMJCORE_READERR_MEMORY_ALLOCATION_FAILED,// 内存申请失败
} lmjcore_read_error_code;

typedef enum{
    // 幽灵对象(最高警报)
    LMJCORE_AUDITERR_GHOST_OBJECT = -1, // 幽灵对象：指针在main库中存在数据但未在arr库注册
    // 幽灵成员(最高警报)
    LMJCORE_AUDITERR_GHOST_MEMBER = -2, // 幽灵成员：在main库中存在但未在arr成员列表中注册
    // 缺失值(警告)
    LMJCORE_AUDITERR_MISSING_VALUE = -3 // 缺失值：在arr库中注册但未在main库赋值
} lmjcore_audit_error_code;

// 17字节指针结构
typedef struct {
    uint8_t data[17];
} lmjcore_ptr;

// 事务句柄（不透明类型）
typedef struct lmjcore_txn lmjcore_txn;

// 环境句柄（不透明类型）
typedef struct lmjcore_env lmjcore_env;

// 指针生成器函数类型
typedef int (*lmjcore_ptr_generator_fn)(void *ctx, uint8_t out[17]);

typedef struct {
    lmjcore_read_error_code code;
    union {
        struct {
            uint16_t member_offset;  // 成员名在缓冲区中的偏移
            uint16_t member_len;     // 成员名长度
        } member;
        struct {
            uint32_t index;          // 数组索引
        } array;
        struct {
            int code;
        } error_code;
    } context;
    lmjcore_ptr entity_ptr;          // 发生错误的实体指针
} lmjcore_read_error;

typedef struct {
    lmjcore_read_error errors[LMJCORE_MAX_READ_ERRORS];
    uint8_t count;
} lmjcore_error_report;

// 成员描述符（完全基于偏移量）
typedef struct {
    uint32_t name_offset;   // 在内存中的偏移
    uint16_t name_len;
    uint32_t value_offset;  // 在内存中的偏移  
    uint16_t value_len;
} lmjcore_member_descriptor;

typedef struct{
    uint32_t value_offset;
    uint16_t value_len;
} lmjcore_value_descriptor;

// 统一返回结构（完全自包含在result_buf中）
typedef struct {
    // 错误报告
    lmjcore_read_error errors[LMJCORE_MAX_READ_ERRORS];
    uint8_t error_count;
    
    // 对象数据布局信息
    size_t count;
    union {
        lmjcore_member_descriptor* members;
        lmjcore_value_descriptor* value;
    }descriptor;
} lmjcore_result;

typedef struct{
    lmjcore_ptr ptr;
    uint8_t member_offset; // 成员名偏移量
    uint8_t member_len;
    lmjcore_audit_error_code error;
} lmjcore_audit_descriptor; 

// 审计报告结构
typedef struct {
    lmjcore_audit_descriptor* audit_descriptor;
    size_t audit_cont; // 报告数量
} lmjcore_audit_report;

// 初始化函数
int lmjcore_init(const char* path, size_t map_size, 
                 lmjcore_ptr_generator_fn ptr_gen, void* ptr_gen_ctx,
                 lmjcore_env** env);

// 清理函数
int lmjcore_cleanup(lmjcore_env* env);

// 事务管理
int lmjcore_txn_begin(lmjcore_env* env, lmjcore_txn_type type, lmjcore_txn** txn);
int lmjcore_txn_commit(lmjcore_txn* txn);
int lmjcore_txn_abort(lmjcore_txn* txn);

// 对象操作
int lmjcore_obj_create(lmjcore_txn* txn, lmjcore_ptr* ptr);
int lmjcore_obj_put(lmjcore_txn* txn, const lmjcore_ptr* obj_ptr, 
                    const uint8_t* member_name, size_t member_name_len,
                    const uint8_t* value, size_t value_len);
int lmjcore_obj_get(lmjcore_txn* txn, const lmjcore_ptr* obj_ptr,
                    lmjcore_query_mode mode,
                    uint8_t* result_buf, size_t result_buf_size,
                    lmjcore_result** result_head);
int lmjcore_obj_member_get(lmjcore_txn* txn,
                    const lmjcore_ptr* obj_ptr,
                    const char* member_name,size_t member_name_len,
                    uint8_t* value_buf, size_t value_buf_size, size_t value_buf_offset,
                    size_t* value_size);

// 对象工具函数
int lmjcore_obj_stat_values(lmjcore_txn* txn,
                        const lmjcore_ptr* obj_ptr,
                        const uint8_t* anchor_member,
                        size_t anchor_member_len,
                        size_t* total_value_len_out,
                        size_t* total_value_count_out);


// 数组操作
int lmjcore_arr_create(lmjcore_txn* txn, lmjcore_ptr* ptr);
int lmjcore_arr_append(lmjcore_txn* txn, const lmjcore_ptr* arr_ptr,
                       const uint8_t* value, size_t value_len);
int lmjcore_arr_get(lmjcore_txn* txn, const lmjcore_ptr* arr_ptr,
                    lmjcore_query_mode mode,
                    uint8_t* result_buf, size_t result_buf_size,
                    lmjcore_result** result_head);

// 数组工具函数
int lmjcore_arr_stat_element(lmjcore_txn* txn,const lmjcore_ptr* ptr,
                        size_t* total_value_len_out,
                        size_t* element_count_out);

// 指针工具函数
int lmjcore_ptr_to_string(const lmjcore_ptr* ptr, char* str_buf, size_t buf_size);
int lmjcore_ptr_from_string(const char* str, lmjcore_ptr* ptr);

// 存在性检查
int lmjcore_exist(lmjcore_txn* txn, const lmjcore_ptr* ptr);

// 审计与修复
int lmjcore_audit_object(lmjcore_txn* txn, const lmjcore_ptr* obj_ptr,
                         uint8_t* report_buf, size_t report_buf_size,
                         lmjcore_audit_report** report_head);
int lmjcore_repair_object(lmjcore_txn* txn, const lmjcore_ptr* obj_ptr,
                          const uint8_t* member_name, size_t member_name_len);

#ifdef __cplusplus
}
#endif

#endif // LMJCORE_H