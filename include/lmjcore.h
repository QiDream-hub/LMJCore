#ifndef LMJCORE_H
#define LMJCORE_H

#include <lmdb.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 错误码定义
#define LMJCORE_SUCCESS 0
#define LMJCORE_ERROR_INVALID_PARAM -32000    // 错误的参数
#define LMJCORE_ERROR_MEMBER_TOO_LONG -32001  // 成员名太长
#define LMJCORE_ERROR_ENTITY_NOT_FOUND -32002 // 实体为找到(无效的指针)
#define LMJCORE_ERROR_GHOST_MEMBER -32003     // 幽灵成员
#define LMJCORE_ERROR_INVALID_POINTER -32004  // 错误的指针(指针格式不对)
#define LMJCORE_ERROR_BUFFER_TOO_SMALL -32005 // 缓冲区太小
#define LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED -32006 // 内存申请错误
#define LMJCORE_ERROR_STRICT_MODE_VIOLATION -32007    // 严格模式读取错误

#define LMJCORE_WRITEMAP MDB_WRITEMAP     // 使用写内存映射（提升性能）
#define LMJCORE_NOSYNC MDB_NOSYNC         // 不强制同步元数据到磁盘
#define LMJCORE_NOMETASYNC MDB_NOMETASYNC // 不强制同步数据到磁盘
#define LMJCORE_MAPASYNC MDB_MAPASYNC     // 使用异步回写
#define LMJCORE_NOTLS MDB_NOTLS           // 不使用线程局部存储

// 固定大小的错误报告
#define LMJCORE_MAX_READ_ERRORS 8

// 指针长度
#define LMJCORE_PTR_LEN 17
// 最长成员名
#define LMJCORE_MAX_MEMBER_NAME_LEN 511 - LMJCORE_PTR_LEN - 1

// 实体类型枚举
typedef enum {
  LMJCORE_OBJ = 0x01, // 对象
  LMJCORE_ARR = 0x02, // 数组
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
  LMJCORE_READERR_ENTITY_NOT_FOUND = 1,     // 实体不存在
  LMJCORE_READERR_MEMBER_MISSING,           // 成员值缺失
  LMJCORE_READERR_BUFFER_TOO_SMALL,         // 缓冲区不足
  LMJCORE_READERR_LMDB_FAILED,              // LMDB操作失败
  LMJCORE_READERR_MEMORY_ALLOCATION_FAILED, // 内存申请失败
} lmjcore_read_error_code;

typedef enum {
  // 幽灵对象(最高警报)
  LMJCORE_AUDITERR_GHOST_OBJECT =
      -1, // 幽灵对象：指针在main库中存在数据但未在arr库注册
  // 幽灵成员(最高警报)
  LMJCORE_AUDITERR_GHOST_MEMBER =
      -2, // 幽灵成员：在main库中存在但未在arr成员列表中注册
  // 缺失值(警告)
  LMJCORE_AUDITERR_MISSING_VALUE = -3 // 缺失值：在arr库中注册但未在main库赋值
} lmjcore_audit_error_code;

// 指针
typedef uint8_t lmjcore_ptr[LMJCORE_PTR_LEN];

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
      uint16_t member_offset; // 成员名在缓冲区中的偏移
      uint16_t member_len;    // 成员名长度
    } member;
    struct {
      uint32_t index; // 数组索引
    } array;
    struct {
      int code;
    } error_code;
  } context;
  lmjcore_ptr entity_ptr; // 发生错误的实体指针
} lmjcore_read_error;

typedef struct {
  lmjcore_read_error errors[LMJCORE_MAX_READ_ERRORS];
  uint8_t count;
} lmjcore_error_report;

// 成员描述符（完全基于偏移量）
typedef struct {
  uint32_t name_offset; // 在内存中的偏移
  uint16_t name_len;
  uint32_t value_offset; // 在内存中的偏移
  uint16_t value_len;
} lmjcore_obj_descriptor;

typedef struct {
  uint32_t value_offset;
  uint16_t value_len;
} lmjcore_arr_descriptor;

// 统一返回结构（完全自包含在result_buf中）
typedef struct {
  // 错误报告
  lmjcore_read_error errors[LMJCORE_MAX_READ_ERRORS];
  uint8_t error_count;

  // 对象数据布局信息
  size_t count;
  union {
    lmjcore_obj_descriptor *object_descriptors;
    lmjcore_arr_descriptor *array_descriptors;
  } descriptor;
} lmjcore_result;

typedef struct {
  lmjcore_ptr ptr;
  size_t member_offset; // 成员名偏移量
  uint16_t member_len;
  lmjcore_audit_error_code error;
} lmjcore_audit_descriptor;

// 审计报告结构
typedef struct {
  lmjcore_audit_descriptor *audit_descriptor;
  size_t audit_cont; // 报告数量
} lmjcore_audit_report;

// 初始化函数
/**
 * @brief 初始化lmjcore
 *
 * @param path 数据库存储路径
 * @param map_size 映射内存大小
 * @param ptr_gen 指针生成函数(可为null,使用默认uuid)
 * @param ptr_gen_ctx 指针生成函数上下文(可为null)
 * @param env lmjcore环境
 * @return int
 */
int lmjcore_init(const char *path, size_t map_size, unsigned int flags,
                 lmjcore_ptr_generator_fn ptr_gen, void *ptr_gen_ctx,
                 lmjcore_env **env);

// 清理函数
/**
 * @brief 清理lmjcore的环境
 *
 * @param env 环境变量
 * @return int
 */
int lmjcore_cleanup(lmjcore_env *env);

// 事务管理
/**
 * @brief 开启一个事务
 *
 * @param env lmjcore环境
 * @param type 事务类型
 * @param txn 事务指针(输出)
 * @return int
 */
int lmjcore_txn_begin(lmjcore_env *env, lmjcore_txn_type type,
                      lmjcore_txn **txn);
/**
 * @brief 提交一个事务
 *
 * @param txn 事务
 * @return int
 */
int lmjcore_txn_commit(lmjcore_txn *txn);
/**
 * @brief 终结一个事务
 *
 * @param txn 事务
 * @return int
 */
int lmjcore_txn_abort(lmjcore_txn *txn);

// 对象操作
/**
 * @brief 创建一个空对象
 *
 * @param txn 写事务
 * @param ptr_out 返回对象指针
 * @return int
 */
int lmjcore_obj_create(lmjcore_txn *txn, lmjcore_ptr ptr_out);
/**
 * @brief 注册指定指针的对象（如果不存在则创建）
 * 
 * 注意:此函数绕过了指针生成这是高危的,请谨慎使用
 *
 * 此函数用于显式声明一个对象指针的有效性。如果该指针在arr数据库中不存在对应条目，
 * 则创建空对象；如果已存在，则验证其有效性。
 *
 * 典型应用场景：
 * - 系统根对象的固定化（如配置根、用户根等）
 * - 迁移场景中重新声明已知指针
 * - 测试环境中构造特定指针的对象
 *
 * @param txn 写事务
 * @param ptr 要注册的对象指针（17字节）
 * @return int 
 *   - LMJCORE_SUCCESS: 注册成功
 *   - LMJCORE_ERROR_INVALID_POINTER: 指针格式错误
 *   - 其他LMDB相关错误码
 */
int lmjcore_obj_register(lmjcore_txn *txn, const lmjcore_ptr ptr);
/**
 * @brief 获取整个对象
 *
 * @param txn 读事务
 * @param obj_ptr 对象指针
 * @param mode 读取模式
 * @param result_buf 返回缓冲区
 * @param result_buf_size 缓冲区大小
 * @param result_head 返回头
 * @return int
 * 缓冲区内存布局定义：
 * +------------------------+ ← result_buf (result)
 * | lmjcore_result         | 固定头部
 * +------------------------+
 * | member_descriptors[]   | 描述符从前向后增长
 * +------------------------+
 * | ...                    |
 * +------------------------+
 * | name & value data      | 数据从后向前增长
 * +------------------------+ ← result_buf + result_buf_size
 */
int lmjcore_obj_get(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                    lmjcore_query_mode mode, uint8_t *result_buf,
                    size_t result_buf_size, lmjcore_result **result_head);
/**
 * @brief 完全删除对象
 *
 * 执行以下操作：
 * 1. 从arr库获取对象的所有成员列表
 * 2. 遍历删除main库中所有成员键值对
 * 3. 删除arr库中的成员列表条目
 * 4. 对象指针变为无效
 *
 * @param txn 写事务
 * @param obj_ptr 对象指针
 * @return int 错误码
 */
int lmjcore_obj_del(lmjcore_txn *txn, const lmjcore_ptr obj_ptr);
/**
 * @brief 获取一个对象成员的值
 *
 * @param txn 读事务
 * @param obj_ptr 对象指针
 * @param member_name 对象成员名称
 * @param member_name_len 成员名长度
 * @param value_buf 成员值缓冲区
 * @param value_buf_size 缓冲区大小
 * @param value_size 成员大小
 * @return int
 */
int lmjcore_obj_member_get(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                           const uint8_t *member_name, size_t member_name_len,
                           uint8_t *value_buf, size_t value_buf_size,
                           size_t *value_size);
/**
 * @brief 提交一个对象成员
 *
 * @param txn 写事务
 * @param obj_ptr 对象指针
 * @param member_name 成员名
 * @param member_name_len 成员名长度
 * @param value 成员的值
 * @param value_len 值的长度
 * @return int
 */
int lmjcore_obj_member_put(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                           const uint8_t *member_name, size_t member_name_len,
                           const uint8_t *value, size_t value_len);

/**
 * @brief 注册对象成员（不设置值）
 *
 * 在arr库的对象成员列表中写入成员名，但在main库中不写入对应的值。
 * 操作后该成员处于"缺失值"(Missing Value)状态，属于合法中间状态。
 *
 * 适用场景：
 * - 结构先行（Schema-first）设计
 * - 分步初始化复杂对象
 * - 预定义对象模板
 *
 * @param txn 写事务
 * @param obj_ptr 对象指针
 * @param member_name 成员名称
 * @param member_name_len 成员名称长度
 * @return int 错误码
 */
int lmjcore_obj_member_register(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                                const uint8_t *member_name,
                                size_t member_name_len);
/**
 * @brief 删除对象成员的值（保留成员注册）
 *
 * 仅删除main数据库中该成员的值，但成员名仍保留在arr库的成员列表中。
 * 操作后该成员状态变为"缺失值"(Missing Value)。
 *
 * @param txn 写事务
 * @param obj_ptr 对象指针
 * @param member_name 成员名称
 * @param member_name_len 成员名称长度
 * @return int 错误码
 */
int lmjcore_obj_member_value_del(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                                 const uint8_t *member_name,
                                 size_t member_name_len);
/**
 * @brief 完全删除对象成员
 *
 * 同时从arr库的成员列表和main库的键值对中删除该成员。
 * 操作后该成员不再属于该对象。
 *
 * @param txn 写事务
 * @param obj_ptr 对象指针
 * @param member_name 成员名称
 * @param member_name_len 成员名称长度
 * @return int 错误码
 */
int lmjcore_obj_member_del(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                           const uint8_t *member_name, size_t member_name_len);

// 对象工具函数
/**
 * @brief 统计一个对象的成员值大小(可能统计到幽灵成员,建议与数组统计配合使用)
 *
 * @param txn 读事务
 * @param obj_ptr 对象指针
 * @param total_value_len_out 值的总长度(可能溢出)
 * @param total_value_count_out 值的总个数
 * @return int
 */
int lmjcore_obj_stat_values(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                            size_t *total_value_len_out,
                            size_t *total_value_count_out);

// 数组操作
/**
 * @brief 创建一个空数组
 *
 * @param txn 写事务
 * @param ptr_out 输出数组指针
 * @return int
 */
int lmjcore_arr_create(lmjcore_txn *txn, lmjcore_ptr ptr_out);
/**
 * @brief 添加一个元素(不验证是否存在)
 *
 * @param txn 写事务
 * @param arr_ptr 数组指针
 * @param value 元素的值
 * @param value_len 元素长度
 * @return int
 */
int lmjcore_arr_append(lmjcore_txn *txn, const lmjcore_ptr arr_ptr,
                       const uint8_t *value, size_t value_len);
/**
 * @brief 获取整个数组
 *
 * @param txn 读事务
 * @param arr_ptr 数组指针
 * @param mode 读取模式
 * @param result_buf 返回缓冲区
 * @param result_buf_size 缓冲区大小
 * @param result_head 返回头
 * @return int
 * 缓冲区内存布局定义：
 * +------------------------+ ← result_buf (result)
 * | lmjcore_result         | 固定头部
 * +------------------------+
 * | descriptors[]          | 描述符从前向后增长
 * +------------------------+
 * | ...                    |
 * +------------------------+
 * | value data             | 数据从后向前增长
 * +------------------------+ ← result_buf + result_buf_size
 */
int lmjcore_arr_get(lmjcore_txn *txn, const lmjcore_ptr arr_ptr,
                    lmjcore_query_mode mode, uint8_t *result_buf,
                    size_t result_buf_size, lmjcore_result **result_head);
/**
 * @brief 完全删除数组
 *
 * 执行以下操作：
 * 1. 删除arr库中该数组指针对应的所有值
 * 2. 数组指针变为无效
 *
 * @param txn 写事务
 * @param arr_ptr 数组指针
 * @return int 错误码
 */
int lmjcore_arr_del(lmjcore_txn *txn, const lmjcore_ptr arr_ptr);
/**
 * @brief 删除数组中的元素
 * 
 * @param txn 写事务
 * @param arr_ptr 数组指针
 * @param element 元素
 * @param element_len 元素长度
 * @return int 
 */
int lmjcore_arr_element_del(lmjcore_txn *txn, const lmjcore_ptr arr_ptr,
                            const uint8_t *element, size_t element_len);
// 数组工具函数
/**
 * @brief 统计数组/成员列表的元素大小
 *
 * @param txn 读事务
 * @param ptr 指针
 * @param total_value_len_out 总长度输出
 * @param element_count_out 元素数量
 * @return int
 */
int lmjcore_arr_stat_element(lmjcore_txn *txn, const lmjcore_ptr ptr,
                             size_t *total_value_len_out,
                             size_t *element_count_out);

// 指针工具函数
/**
 * @brief lmjcore指针转字符串
 *
 * @param ptr 指针
 * @param str_buf 字符串缓冲区
 * @param buf_size 缓冲区大小
 * @return int
 */
int lmjcore_ptr_to_string(const lmjcore_ptr ptr, char *str_buf,
                          size_t buf_size);
/**
 * @brief 字符串转lmjcore指针
 *
 * @param str 字符串
 * @param ptr_out 指针输出
 * @return int
 */
int lmjcore_ptr_from_string(const char *str, lmjcore_ptr ptr_out);

// 存在性检查
/**
 * @brief 实体探针(检查实体[对象/数组]是否存在)
 *
 * @param txn 读事务
 * @param ptr 指针
 * @return int
 */
int lmjcore_entity_exist(lmjcore_txn *txn, const lmjcore_ptr ptr);
/**
 * @brief 对象成员值探针(检查对象成员的值是否存在)
 *
 * @param txn 读事务
 * @param obj_ptr 对象指针
 * @param member_name 成员名
 * @param member_name_len 成语名长度
 * @return int
 */
int lmjcore_obj_member_value_exist(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                                   const uint8_t *member_name,
                                   size_t member_name_len);

// 审计与修复
/**
 * @brief 审计一个对象(检查对象是否存在幽灵成员,缺失值)
 *
 * @param txn 读事务
 * @param obj_ptr 对象指针
 * @param report_buf 审计报告缓冲区
 * @param report_buf_size 缓冲区大小
 * @param report_head 缓冲区头
 * @return int
 * 审计报告内存布局
 * report_buf内存结构
 * ---------------------  <-- report_buf(report_head)
 * |report_head         |
 * ---------------------
 * |report_descriptor   | <-- 前往后
 * ----------------------
 * |....                |
 * ----------------------
 * | member_name        | <-- 由后往前
 * ---------------------- <-- data_offset(report_buf_size)
 */
int lmjcore_audit_object(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                         uint8_t *report_buf, size_t report_buf_size,
                         lmjcore_audit_report **report_head);
/**
 * @brief
 * 使用审计报告修复幽灵成员(删除幽灵成员).如果需要重新将幽灵成员注册为成员需要手动注册,因为内核无法判断幽灵成员的值是否可用
 *
 * @param txn 读事务
 * @param report_buf 报告缓冲区
 * @param report_buf_size 缓冲区大小
 * @param report 报告
 * @return int
 */
int lmjcore_repair_object(lmjcore_txn *txn, uint8_t *report_buf,
                          size_t report_buf_size, lmjcore_audit_report *report);

// 其他工具
/**
 * @brief 用于判断事务的类型
 * 
 * @param txn 事务
 * @param type 事务类型
 * @return true 
 * @return false 
 */
bool is_txn_type(lmjcore_txn *txn,lmjcore_txn_type type);
/**
 * @brief 用于判读实体类型
 * 
 * @param ptr 实体指针
 * @param type 实体类型
 * @return true 
 * @return false 
 */
bool is_entity_type(lmjcore_ptr ptr,lmjcore_entity_type type);
#ifdef __cplusplus
}
#endif

#endif // LMJCORE_H