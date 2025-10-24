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
#define LMJCORE_ERROR_INVALID_PARAM -32000            // 参数无效
#define LMJCORE_ERROR_MEMBER_TOO_LONG -32001          // 成员名长度超限
#define LMJCORE_ERROR_ENTITY_NOT_FOUND -32002         // 实体不存在（无效指针）
#define LMJCORE_ERROR_GHOST_MEMBER -32003             // 检测到幽灵成员
#define LMJCORE_ERROR_INVALID_POINTER -32004          // 指针格式错误
#define LMJCORE_ERROR_BUFFER_TOO_SMALL -32005         // 输出缓冲区空间不足
#define LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED -32006 // 内存分配失败
#define LMJCORE_ERROR_STRICT_MODE_VIOLATION -32007    // 严格模式校验失败
#define LMJCORE_ERROR_MEMBER_NOT_FOUND -32008         // 成员值不存在(缺失值)

// 环境标志位（映射 LMDB 标志）
/**
 * LMDB 环境标志位完整映射
 * 参考：https://lmdb.readthedocs.io/en/release/#environment-flags
 */

// 文件操作相关标志
#define LMJCORE_FLAGS_FIXEDMAP    MDB_FIXEDMAP    // 使用固定内存映射
#define LMJCORE_FLAGS_NOSUBDIR    MDB_NOSUBDIR    // 环境文件直接放在路径中，不创建子目录
#define LMJCORE_FLAGS_NOSYNC      MDB_NOSYNC      // 不强制将数据同步到磁盘（写入后立即返回）
#define LMJCORE_FLAGS_RDONLY      MDB_RDONLY      // 以只读模式打开环境
#define LMJCORE_FLAGS_NOMETASYNC  MDB_NOMETASYNC  // 不强制在提交后同步元数据
#define LMJCORE_FLAGS_WRITEMAP    MDB_WRITEMAP    // 使用写时内存映射（提升写入性能）
#define LMJCORE_FLAGS_MAPASYNC    MDB_MAPASYNC    // 在同步模式下使用异步回写
#define LMJCORE_FLAGS_NOTLS       MDB_NOTLS       // 禁用线程局部存储

// 数据库操作相关标志
#define LMJCORE_FLAGS_NOLOCK      MDB_NOLOCK      // 不使用任何锁（实验性）
#define LMJCORE_FLAGS_NORDAHEAD   MDB_NORDAHEAD   // 禁用预读
#define LMJCORE_FLAGS_NOMEMINIT   MDB_NOMEMINIT   // 不初始化内存（提升性能，但有安全风险）

// 组合标志（常用配置模式）
#define LMJCORE_FLAGS_SAFE_SYNC       0                                     // 安全同步模式（默认）
#define LMJCORE_FLAGS_MAX_PERF        (LMJCORE_FLAGS_WRITEMAP | LMJCORE_FLAGS_MAPASYNC) // 最大性能模式
#define LMJCORE_FLAGS_READONLY_MODE   LMJCORE_FLAGS_RDONLY                        // 只读模式
#define LMJCORE_FLAGS_NO_DISK_SYNC    (LMJCORE_FLAGS_NOSYNC | LMJCORE_FLAGS_NOMETASYNC) // 无磁盘同步模式

// 兼容性标志（不同版本的 LMDB）
#ifdef MDB_PREVSNAPSHOT
#define LMJCORE_PREVSNAPSHOT    MDB_PREVSNAPSHOT  // 允许从先前快照读取
#endif

// 固定容量的错误报告上限
#define LMJCORE_MAX_READ_ERRORS 8

// key总长度
#define LMJCORE_MAX_KEY_LEN 511
// 指针长度（字节）
#define LMJCORE_PTR_LEN 17
// 成员名最大长度（基于 LMDB 键长限制计算）
#define LMJCORE_MAX_MEMBER_NAME_LEN LMJCORE_MAX_KEY_LEN - LMJCORE_PTR_LEN - 1

// 实体类型枚举
typedef enum {
  LMJCORE_OBJ = 0x01, // 对象类型
  LMJCORE_ARR = 0x02, // 数组类型
} lmjcore_entity_type;

// 查询模式枚举
typedef enum {
  LMJCORE_MODE_LOOSE = 0,  // 宽松模式：缺失值自动填充
  LMJCORE_MODE_STRICT = 1, // 严格模式：遇缺失立即报错
} lmjcore_query_mode;

// 事务类型枚举
typedef enum {
  LMJCORE_TXN_READONLY = 0, // 只读事务
  LMJCORE_TXN_WRITE = 1,    // 读写事务
} lmjcore_txn_type;

// 读取错误代码枚举
typedef enum {
  LMJCORE_READERR_NONE = 0,
  LMJCORE_READERR_ENTITY_NOT_FOUND = 1,     // 目标实体不存在
  LMJCORE_READERR_MEMBER_MISSING,           // 对象成员值缺失
  LMJCORE_READERR_BUFFER_TOO_SMALL,         // 输出缓冲区不足
  LMJCORE_READERR_LMDB_FAILED,              // LMDB 底层操作失败
  LMJCORE_READERR_MEMORY_ALLOCATION_FAILED, // 内存分配失败
} lmjcore_read_error_code;

// 审计错误代码枚举
typedef enum {
  LMJCORE_AUDITERR_GHOST_OBJECT =
      -1, // 幽灵对象：指针在 main 库有数据但未在 arr 库注册
  LMJCORE_AUDITERR_GHOST_MEMBER =
      -2, // 幽灵成员：在 main 库存在但未在 arr 成员列表中注册
  LMJCORE_AUDITERR_MISSING_VALUE =
      -3, // 缺失值：在 arr 库注册但未在 main 库赋值
} lmjcore_audit_error_code;

// 17字节实体指针类型
typedef uint8_t lmjcore_ptr[LMJCORE_PTR_LEN];

// 事务句柄（不透明结构）
typedef struct lmjcore_txn lmjcore_txn;

// 环境句柄（不透明结构）
typedef struct lmjcore_env lmjcore_env;

// 指针生成器函数类型
typedef int (*lmjcore_ptr_generator_fn)(void *ctx, uint8_t out[17]);

// 读取错误详情结构
typedef struct {
  lmjcore_read_error_code code;
  union {
    struct {
      uint16_t element_offset; // 成员名或数组元素在缓冲区中的偏移量
      uint16_t element_len;    // 成员名或数组元素长度
    } element;
    struct {
      int code;
    } error_code;
  } context;
  lmjcore_ptr entity_ptr; // 发生错误的实体指针
} lmjcore_read_error;

// 对象成员描述符（基于偏移量定位）
typedef struct {
  uint32_t name_offset;  // 成员名在缓冲区中的偏移
  uint16_t name_len;     // 成员名长度
  uint32_t value_offset; // 成员值在缓冲区中的偏移
  uint16_t value_len;    // 成员值长度
} lmjcore_obj_descriptor;

// 数组元素描述符
typedef struct {
  uint32_t value_offset; // 元素值在缓冲区中的偏移
  uint16_t value_len;    // 元素值长度
} lmjcore_arr_descriptor;

// 统一查询结果结构（完全自包含于调用方提供的缓冲区）
typedef struct {
  lmjcore_read_error errors[LMJCORE_MAX_READ_ERRORS]; // 错误列表
  uint8_t error_count;                                // 错误数量

  size_t count; // 成员或元素数量
  union {
    lmjcore_obj_descriptor *object_descriptors; // 对象成员描述符数组
    lmjcore_arr_descriptor *array_descriptors;  // 数组元素描述符数组
  } descriptor;
} lmjcore_result;

// 审计条目描述符
typedef struct {
  lmjcore_ptr ptr;                // 相关实体指针
  size_t member_offset;           // 成员名偏移量
  uint16_t member_len;            // 成员名长度
  lmjcore_audit_error_code error; // 审计错误类型
} lmjcore_audit_descriptor;

// 审计报告结构
typedef struct {
  lmjcore_audit_descriptor *audit_descriptor; // 审计条目数组
  size_t audit_cont;                          // 审计条目数量
} lmjcore_audit_report;

// ==================== 初始化与清理 ====================

/**
 * @brief 初始化 LMJCore 环境
 *
 * @param path 数据库存储目录路径
 * @param map_size 内存映射大小（字节）
 * @param flags 环境标志位（LMJCORE_* 组合）
 * @param ptr_gen 自定义指针生成器（NULL 则使用默认 UUIDv4）
 * @param ptr_gen_ctx 指针生成器的上下文数据（可为 NULL）
 * @param env 输出参数，返回初始化后的环境句柄
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_init(const char *path, size_t map_size, unsigned int flags,
                 lmjcore_ptr_generator_fn ptr_gen, void *ptr_gen_ctx,
                 lmjcore_env **env);

/**
 * @brief 清理并释放 LMJCore 环境资源
 *
 * @param env 待清理的环境句柄
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_cleanup(lmjcore_env *env);

// ==================== 事务管理 ====================

/**
 * @brief 开启一个新事务
 *
 * @param env 已初始化的环境句柄
 * @param type 事务类型（只读或读写）
 * @param txn 输出参数，返回创建的事务句柄
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_txn_begin(lmjcore_env *env, lmjcore_txn_type type,
                      lmjcore_txn **txn);

/**
 * @brief 提交事务，使所有修改持久化
 *
 * @param txn 待提交的事务句柄（提交后失效）
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_txn_commit(lmjcore_txn *txn);

/**
 * @brief 中止事务，丢弃所有未提交的修改
 *
 * @param txn 待中止的事务句柄（中止后失效）
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_txn_abort(lmjcore_txn *txn);

// ==================== 对象操作 ====================

/**
 * @brief 创建一个空对象实体
 *
 * @param txn 有效的写事务句柄
 * @param ptr_out 输出参数，返回新对象的指针
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_obj_create(lmjcore_txn *txn, lmjcore_ptr ptr_out);

/**
 * @brief 注册指定指针的对象（如不存在则创建空对象）
 *
 * 此函数为高级操作，绕过默认指针生成机制，请谨慎使用。
 * 典型应用场景：
 * - 系统根对象的固定化（如配置根、用户根等）
 * - 迁移场景中重新声明已知指针
 * - 测试环境中构造特定指针的对象
 *
 * @param txn 有效的写事务句柄
 * @param ptr 待注册的 17 字节对象指针
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_obj_register(lmjcore_txn *txn, const lmjcore_ptr ptr);

/**
 * @brief 获取指定对象的完整内容
 *
 * 结果将写入调用方提供的缓冲区，内存布局如下：
 * +------------------------+ ← result_buf
 * | lmjcore_result         | 固定头部
 * +------------------------+
 * | member_descriptors[]   | 描述符数组（从前向后增长）
 * +------------------------+
 * | ...                    |
 * +------------------------+
 * | name & value data      | 名称和值数据（从后向前增长）
 * +------------------------+ ← result_buf + result_buf_size
 *
 * @param txn 有效的读事务句柄
 * @param obj_ptr 目标对象指针
 * @param mode 读取模式（宽松/严格）
 * @param result_buf 输出缓冲区
 * @param result_buf_size 输出缓冲区大小
 * @param result_head 输出参数，指向结果头部的指针
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_obj_get(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                    lmjcore_query_mode mode, uint8_t *result_buf,
                    size_t result_buf_size, lmjcore_result **result_head);

/**
 * @brief 完全删除指定对象及其所有成员
 *
 * 执行以下操作：
 * 1. 从 arr 库获取对象的所有成员列表
 * 2. 遍历删除 main 库中所有成员键值对
 * 3. 删除 arr 库中的成员列表条目
 * 4. 对象指针变为无效
 *
 * @param txn 有效的写事务句柄
 * @param obj_ptr 待删除的对象指针
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_obj_del(lmjcore_txn *txn, const lmjcore_ptr obj_ptr);

/**
 * @brief 获取对象指定成员的值
 *
 * @param txn 有效的读事务句柄
 * @param obj_ptr 目标对象指针
 * @param member_name 成员名称（二进制安全字节序列）
 * @param member_name_len 成员名称长度
 * @param value_buf 输出缓冲区，用于接收成员值
 * @param value_buf_size 输出缓冲区大小
 * @param value_size 输出参数，返回实际值的长度
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_obj_member_get(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                           const uint8_t *member_name, size_t member_name_len,
                           uint8_t *value_buf, size_t value_buf_size,
                           size_t *value_size);

/**
 * @brief 设置或更新对象成员的值
 *
 * @param txn 有效的写事务句柄
 * @param obj_ptr 目标对象指针
 * @param member_name 成员名称（二进制安全字节序列）
 * @param member_name_len 成员名称长度
 * @param value 待写入的成员值（二进制安全字节序列）
 * @param value_len 值的长度
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_obj_member_put(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                           const uint8_t *member_name, size_t member_name_len,
                           const uint8_t *value, size_t value_len);

/**
 * @brief 注册对象成员（仅注册名称，不设置值）
 *
 * 在 arr 库的对象成员列表中写入成员名，但在 main 库中不写入对应的值。
 * 操作后该成员处于"缺失值"(Missing Value)状态，属于合法中间状态。
 * 适用场景：
 * - 结构先行（Schema-first）设计
 * - 分步初始化复杂对象
 * - 预定义对象模板
 *
 * @param txn 有效的写事务句柄
 * @param obj_ptr 目标对象指针
 * @param member_name 成员名称（二进制安全字节序列）
 * @param member_name_len 成员名称长度
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_obj_member_register(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                                const uint8_t *member_name,
                                size_t member_name_len);

/**
 * @brief 删除对象成员的值（保留成员注册）
 *
 * 仅删除 main 数据库中该成员的值，但成员名仍保留在 arr 库的成员列表中。
 * 操作后该成员状态变为"缺失值"(Missing Value)。
 *
 * @param txn 有效的写事务句柄
 * @param obj_ptr 目标对象指针
 * @param member_name 成员名称（二进制安全字节序列）
 * @param member_name_len 成员名称长度
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_obj_member_value_del(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                                 const uint8_t *member_name,
                                 size_t member_name_len);

/**
 * @brief 完全删除对象成员（包括注册信息）
 *
 * 同时从 arr 库的成员列表和 main 库的键值对中删除该成员。
 * 操作后该成员不再属于该对象。
 *
 * @param txn 有效的写事务句柄
 * @param obj_ptr 目标对象指针
 * @param member_name 成员名称（二进制安全字节序列）
 * @param member_name_len 成员名称长度
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_obj_member_del(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                           const uint8_t *member_name, size_t member_name_len);

// ==================== 对象工具函数 ====================

/**
 * @brief 统计对象所有成员值的总长度和数量
 *
 * 注意：可能统计到幽灵成员，建议与数组统计配合进行完整性校验。
 *
 * @param txn 有效的读事务句柄
 * @param obj_ptr 目标对象指针
 * @param total_value_len_out 输出参数，成员值总长度
 * @param total_value_count_out 输出参数，成员值总数量
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_obj_stat_values(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                            size_t *total_value_len_out,
                            size_t *total_value_count_out);

// ==================== 数组操作 ====================

/**
 * @brief 创建一个空数组实体
 *
 * @param txn 有效的写事务句柄
 * @param ptr_out 输出参数，返回新数组的指针
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_arr_create(lmjcore_txn *txn, lmjcore_ptr ptr_out);

/**
 * @brief 向数组末尾追加一个元素
 *
 * @param txn 有效的写事务句柄
 * @param arr_ptr 目标数组指针
 * @param value 待追加的元素值（二进制安全字节序列）
 * @param value_len 元素值长度
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_arr_append(lmjcore_txn *txn, const lmjcore_ptr arr_ptr,
                       const uint8_t *value, size_t value_len);

/**
 * @brief 获取指定数组的完整内容
 *
 * 结果将写入调用方提供的缓冲区，内存布局如下：
 * +------------------------+ ← result_buf
 * | lmjcore_result         | 固定头部
 * +------------------------+
 * | descriptors[]          | 元素描述符数组（从前向后增长）
 * +------------------------+
 * | ...                    |
 * +------------------------+
 * | value data             | 元素值数据（从后向前增长）
 * +------------------------+ ← result_buf + result_buf_size
 *
 * @param txn 有效的读事务句柄
 * @param arr_ptr 目标数组指针
 * @param mode 读取模式（宽松/严格）
 * @param result_buf 输出缓冲区
 * @param result_buf_size 输出缓冲区大小
 * @param result_head 输出参数，指向结果头部的指针
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_arr_get(lmjcore_txn *txn, const lmjcore_ptr arr_ptr,
                    lmjcore_query_mode mode, uint8_t *result_buf,
                    size_t result_buf_size, lmjcore_result **result_head);

/**
 * @brief 完全删除指定数组及其所有元素
 *
 * 执行以下操作：
 * 1. 删除 arr 库中该数组指针对应的所有元素
 * 2. 数组指针变为无效
 *
 * @param txn 有效的写事务句柄
 * @param arr_ptr 待删除的数组指针
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_arr_del(lmjcore_txn *txn, const lmjcore_ptr arr_ptr);

/**
 * @brief 删除数组中指定的元素（按值匹配）
 *
 * @param txn 有效的写事务句柄
 * @param arr_ptr 目标数组指针
 * @param element 待删除的元素值（二进制安全字节序列）
 * @param element_len 元素值长度
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_arr_element_del(lmjcore_txn *txn, const lmjcore_ptr arr_ptr,
                            const uint8_t *element, size_t element_len);

// ==================== 数组工具函数 ====================

/**
 * @brief 统计数组或成员列表的元素总长度和数量
 *
 * @param txn 有效的读事务句柄
 * @param ptr 目标指针（数组或对象）
 * @param total_value_len_out 输出参数，元素值总长度
 * @param element_count_out 输出参数，元素总数量
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_arr_stat_element(lmjcore_txn *txn, const lmjcore_ptr ptr,
                             size_t *total_value_len_out,
                             size_t *element_count_out);

// ==================== 指针工具函数 ====================

/**
 * @brief 将 17 字节指针转换为可读字符串格式
 *
 * @param ptr 待转换的指针
 * @param str_buf 输出缓冲区，用于接收字符串
 * @param buf_size 输出缓冲区大小（至少 35 字节）
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_ptr_to_string(const lmjcore_ptr ptr, char *str_buf,
                          size_t buf_size);

/**
 * @brief 将字符串格式的指针转换回 17 字节二进制格式
 *
 * @param str 待转换的字符串
 * @param ptr_out 输出参数，返回二进制指针
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_ptr_from_string(const char *str, lmjcore_ptr ptr_out);

// ==================== 存在性检查 ====================

/**
 * @brief 检查指定指针是否指向有效实体
 *
 * @param txn 有效的读事务句柄
 * @param ptr 待检查的指针
 * @return int
 *   - 1: 实体存在
 *   - 0: 实体不存在
 *   - <0: 操作失败，返回错误码
 */
int lmjcore_entity_exist(lmjcore_txn *txn, const lmjcore_ptr ptr);

/**
 * @brief 检查对象成员的值是否存在
 *
 * @param txn 有效的读事务句柄
 * @param obj_ptr 目标对象指针
 * @param member_name 成员名称（二进制安全字节序列）
 * @param member_name_len 成员名称长度
 * @return int
 *   - 1: 成员值存在
 *   - 0: 成员值不存在（缺失值状态）
 *   - <0: 操作失败，返回错误码
 */
int lmjcore_obj_member_value_exist(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                                   const uint8_t *member_name,
                                   size_t member_name_len);

// ==================== 审计与修复 ====================

/**
 * @brief 审计指定对象的数据完整性
 *
 * 检查对象是否存在幽灵成员、缺失值等问题。
 * 审计报告将写入调用方提供的缓冲区，内存布局如下：
 * +------------------------+ ← report_buf
 * | report_head           | 报告头部
 * +------------------------+
 * | report_descriptor     | 审计描述符数组（从前向后增长）
 * +------------------------+
 * | ...                   |
 * +------------------------+
 * | member_name           | 成员名称数据（从后向前增长）
 * +------------------------+ ← report_buf + report_buf_size
 *
 * @param txn 有效的读事务句柄
 * @param obj_ptr 待审计的对象指针
 * @param report_buf 输出缓冲区
 * @param report_buf_size 输出缓冲区大小
 * @param report_head 输出参数，指向报告头部的指针
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_audit_object(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                         uint8_t *report_buf, size_t report_buf_size,
                         lmjcore_audit_report **report_head);

/**
 * @brief 使用审计报告修复对象中的幽灵成员
 *
 * 安全删除幽灵成员在 main 数据库中的键值对。
 * 注意：此操作不会将幽灵成员重新注册为合法成员，需由上层根据业务逻辑判断。
 *
 * @param txn 有效的写事务句柄
 * @param report_buf 审计报告缓冲区
 * @param report_buf_size 审计报告缓冲区大小
 * @param report 审计报告指针
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_repair_object(lmjcore_txn *txn, uint8_t *report_buf,
                          size_t report_buf_size, lmjcore_audit_report *report);

// ==================== 类型判断工具 ====================

/**
 * @brief 判断事务的类型
 *
 * @param txn 事务句柄
 * @param type 待比较的事务类型
 * @return true 事务类型匹配
 * @return false 事务类型不匹配或句柄无效
 */
bool is_txn_type(lmjcore_txn *txn, lmjcore_txn_type type);

/**
 * @brief 判断实体指针的类型
 *
 * @param ptr 实体指针
 * @param type 待比较的实体类型
 * @return true 实体类型匹配
 * @return false 实体类型不匹配或指针无效
 */
bool is_entity_type(lmjcore_ptr ptr, lmjcore_entity_type type);

#ifdef __cplusplus
}
#endif

#endif // LMJCORE_H