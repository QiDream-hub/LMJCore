#ifndef LMJCORE_H
#define LMJCORE_H

#include <lmdb.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 错误码定义 ====================
// 基础错误码范围：-32000 ~ -32999

#define LMJCORE_SUCCESS 0

// 参数相关 (-32000 ~ -32019)
#define LMJCORE_ERROR_INVALID_PARAM -32000    // 通用参数无效
#define LMJCORE_ERROR_NULL_POINTER -32001     // 空指针
#define LMJCORE_ERROR_INVALID_POINTER -32003  // 指针格式错误
#define LMJCORE_ERROR_MEMBER_TOO_LONG -32002  // 成员名长度超限
#define LMJCORE_ERROR_BUFFER_TOO_SMALL -32004 // 输出缓冲区空间不足

// 事务相关 (-32020 ~ -32039)
#define LMJCORE_ERROR_READONLY_TXN -32020    // 在只读事务中执行写操作
#define LMJCORE_ERROR_READONLY_PARENT -32021 // 父事务是只读，不能开子事务

// 实体相关 (-32040 ~ -32059)
#define LMJCORE_ERROR_ENTITY_NOT_FOUND -32040     // 实体不存在
#define LMJCORE_ERROR_ENTITY_EXISTS -32041        // 实体已存在
#define LMJCORE_ERROR_ENTITY_TYPE_MISMATCH -32042 // 实体类型不匹配

// 成员相关 (-32060 ~ -32079)
#define LMJCORE_ERROR_MEMBER_NOT_FOUND -32060 // 成员不存在
#define LMJCORE_ERROR_MEMBER_EXISTS -32061    // 成员已存在
#define LMJCORE_ERROR_MEMBER_MISSING -32062   // 成员缺失值

// 资源相关 (-32080 ~ -32099)
#define LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED -32080 // 内存分配失败

// 审计相关 (-32100 ~ -32119)
#define LMJCORE_ERROR_GHOST_MEMBER -32100 // 存在幽灵成员

// ==================== 环境标志 ====================

/**
 * @def LMJCORE_ENV_NOSYNC
 * @brief 不强制同步到磁盘（MDB_NOSYNC）
 *
 * 提交事务后不强制 fsync，由操作系统决定何时写盘。
 * 提升性能，但系统崩溃可能丢失最近的事务。
 */
#define LMJCORE_ENV_NOSYNC MDB_NOSYNC

/**
 * @def LMJCORE_ENV_NOMETASYNC
 * @brief 不同步元数据（MDB_NOMETASYNC）
 *
 * 提交事务时不同步元数据页，只同步数据页。
 * 配合 MDB_NOSYNC 使用可进一步提升性能。
 */
#define LMJCORE_ENV_NOMETASYNC MDB_NOMETASYNC

/**
 * @def LMJCORE_ENV_WRITEMAP
 * @brief 使用可写内存映射（MDB_WRITEMAP）
 *
 * 直接写入内存映射文件，减少数据拷贝，大幅提升写入性能。
 * 需配合 MDB_MAPASYNC 使用以避免数据损坏风险。
 */
#define LMJCORE_ENV_WRITEMAP MDB_WRITEMAP

/**
 * @def LMJCORE_ENV_MAPASYNC
 * @brief 异步内存同步（MDB_MAPASYNC）
 *
 * 使用 MDB_WRITEMAP 时，以异步方式将修改写回磁盘。
 * 进一步提升性能，但系统崩溃可能丢失最近写入。
 */
#define LMJCORE_ENV_MAPASYNC MDB_MAPASYNC

/**
 * @def LMJCORE_ENV_NOLOCK
 * @brief 无锁模式（MDB_NOLOCK）
 *
 * 禁用 LMDB 内部锁机制，调用者必须自行管理并发访问。
 * 适用于单进程访问或外部已有锁机制的场景。
 */
#define LMJCORE_ENV_NOLOCK MDB_NOLOCK

/**
 * @def LMJCORE_ENV_NOTLS
 * @brief 禁用线程局部存储（MDB_NOTLS）
 *
 * 不将读事务槽位绑定到线程，允许在一个线程中开启事务，
 * 在另一个线程中提交/中止。用于特定的线程模型。
 */
#define LMJCORE_ENV_NOTLS MDB_NOTLS

/**
 * @def LMJCORE_ENV_NORDAHEAD
 * @brief 禁用预读（MDB_NORDAHEAD）
 *
 * 关闭 LMDB 的文件预读功能，适用于随机访问模式。
 * 在 Windows 上无效果。
 */
#define LMJCORE_ENV_NORDAHEAD MDB_NORDAHEAD

/**
 * @def LMJCORE_ENV_NOMEMINIT
 * @brief 不初始化内存（MDB_NOMEMINIT）
 *
 * 分配内存时不初始化为零，可提升性能但有安全风险。
 * 仅当确信不会读取未初始化的数据时使用。
 */
#define LMJCORE_ENV_NOMEMINIT MDB_NOMEMINIT

/**
 * @def LMJCORE_ENV_FIXEDMAP
 * @brief 固定内存映射地址（MDB_FIXEDMAP）
 *
 * 尝试在固定地址进行内存映射，实验性功能。
 * 通常不需要使用。
 */
#define LMJCORE_ENV_FIXEDMAP MDB_FIXEDMAP

/**
 * @def LMJCORE_ENV_NOSUBDIR
 * @brief 不创建子目录（MDB_NOSUBDIR）
 *
 * 直接在指定路径创建数据文件，而不是在 path/data.mdb。
 * 适用于嵌入式场景。
 */
#define LMJCORE_ENV_NOSUBDIR MDB_NOSUBDIR

/**
 * @def LMJCORE_ENV_READONLY
 * @brief 只读模式（MDB_RDONLY）
 *
 * 以只读方式打开环境，不允许任何写入操作。
 * 可用于多进程共享读。
 */
#define LMJCORE_ENV_READONLY MDB_RDONLY

// ==================== 事务标志 ====================

/**
 * @def LMJCORE_TXN_READONLY
 * @brief 只读事务（MDB_RDONLY）
 *
 * 事务只能执行读取操作，任何写入尝试都会失败。
 * 只读事务可以并发执行，不会阻塞写事务。
 */
#define LMJCORE_TXN_READONLY MDB_RDONLY

/**
 * @def LMJCORE_TXN_NOSYNC
 * @brief 事务级不强制同步（MDB_NOSYNC）
 *
 * 覆盖环境设置，当前事务提交时不强制 fsync。
 * 用于特定事务的性能优化。
 */
#define LMJCORE_TXN_NOSYNC MDB_NOSYNC

/**
 * @def LMJCORE_TXN_NOMETASYNC
 * @brief 事务级元数据异步（MDB_NOMETASYNC）
 *
 * 覆盖环境设置，当前事务提交时不同步元数据。
 */
#define LMJCORE_TXN_NOMETASYNC MDB_NOMETASYNC

/**
 * @def LMJCORE_TXN_MAPASYNC
 * @brief 事务级异步映射（MDB_MAPASYNC）
 *
 * 覆盖环境设置，当前事务使用异步内存同步。
 */
#define LMJCORE_TXN_MAPASYNC MDB_MAPASYNC

/**
 * @def LMJCORE_TXN_NOTLS
 * @brief 事务级禁用 TLS（MDB_NOTLS）
 *
 * 覆盖环境设置，当前事务不绑定到线程。
 */
#define LMJCORE_TXN_NOTLS MDB_NOTLS

// ==================== 便捷组合 ====================

/**
 * @def LMJCORE_ENV_MAX_PERF
 * @brief 最大性能配置
 *
 * 组合所有性能优化标志，提供极致写入性能。
 * 警告：系统崩溃可能丢失数据！
 */
#define LMJCORE_ENV_MAX_PERF                                                   \
  (LMJCORE_ENV_WRITEMAP | LMJCORE_ENV_MAPASYNC | LMJCORE_ENV_NOSYNC |          \
   LMJCORE_ENV_NOMETASYNC)

/**
 * @def LMJCORE_ENV_SAFE
 * @brief 安全模式（默认）
 *
 * 不添加任何性能优化标志，保证最高级别的数据安全。
 * 每个事务提交都会强制刷盘。
 */
#define LMJCORE_ENV_SAFE 0

/**
 * @def LMJCORE_TXN_HIGH_PERF
 * @brief 事务高性能模式
 *
 * 为当前事务启用性能优化，牺牲持久性换取速度。
 */
#define LMJCORE_TXN_HIGH_PERF (LMJCORE_TXN_NOSYNC | LMJCORE_TXN_NOMETASYNC)

/**
 * @def LMJCORE_TXN_DEFAULT
 * @brief 默认事务（0标志）
 *
 * 遵循环境设置的事务行为。
 */
#define LMJCORE_TXN_DEFAULT 0

// ==================== lmjcore常量定义 ====================
// 固定容量的错误报告上限
#define LMJCORE_MAX_READ_ERRORS 8

// key总长度
#define LMJCORE_MAX_KEY_LEN 511
// 指针长度（字节）
#define LMJCORE_PTR_LEN 17
// 成员名最大长度（基于 LMDB 键长限制计算）
#define LMJCORE_MAX_MEMBER_NAME_LEN (LMJCORE_MAX_KEY_LEN - LMJCORE_PTR_LEN - 1)

// 指针字符串长度
#define LMJCORE_PTR_STRING_LEN (LMJCORE_PTR_LEN * 2)
#define LMJCORE_PTR_STRING_BUF_SIZE (LMJCORE_PTR_STRING_LEN + 1)

// 实体类型枚举
typedef enum {
  LMJCORE_OBJ = 0x01, // 对象类型
  LMJCORE_ARR = 0x02, // 数组类型
} lmjcore_entity_type;

// 17字节实体指针类型
typedef uint8_t lmjcore_ptr[LMJCORE_PTR_LEN];

// 事务句柄（不透明结构）
typedef struct lmjcore_txn lmjcore_txn;

// 环境句柄（不透明结构）
typedef struct lmjcore_env lmjcore_env;

// 指针生成器函数类型
typedef int (*lmjcore_ptr_generator_fn)(void *ctx,
                                        uint8_t out[LMJCORE_PTR_LEN]);

// 读取错误详情结构
typedef struct {
  unsigned int error_code; // 错误码
  struct {
    size_t element_offset; // 成员名或数组元素在缓冲区中的偏移量
    size_t element_len;    // 成员名或数组元素长度
  } element;
  lmjcore_ptr entity_ptr; // 发生错误的实体指针
} lmjcore_read_error;

// 描述符基础展示单元(一个数组的元素,一个对象的成员名,或者成员的值)
typedef struct {
  size_t value_offset; // 元素值在缓冲区中的偏移
  size_t value_len;    // 元素值长度
} lmjcore_descriptor;

// 一个完整的成员
typedef struct {
  lmjcore_descriptor member_name;
  lmjcore_descriptor member_value;
} lmjcore_member_descriptor;

// 对象返回体
typedef struct {
  size_t error_count;                                 // 错误统计
  lmjcore_read_error errors[LMJCORE_MAX_READ_ERRORS]; // 错误数组

  size_t member_count; // 成员统计
  lmjcore_member_descriptor members[];

} lmjcore_result_obj;

// 数组返回体
typedef struct {
  size_t error_count;                                 // 错误统计
  lmjcore_read_error errors[LMJCORE_MAX_READ_ERRORS]; // 错误数组

  size_t element_count; // 元素统计
  lmjcore_descriptor elements[];
} lmjcore_result_arr;

// 审计条目描述符
typedef struct {
  lmjcore_ptr ptr;                  // 相关实体指针
  lmjcore_member_descriptor member; // 幽灵成员
} lmjcore_audit_descriptor;

// 审计报告结构
typedef struct {
  size_t audit_cont;                           // 审计条目数量
  lmjcore_audit_descriptor audit_descriptor[]; // 审计条目数组
} lmjcore_audit_report;

// ==================== 初始化与清理 ====================

/**
 * @brief 初始化 LMJCore 环境
 *
 * @param path 数据库存储目录路径
 * @param map_size 内存映射大小（字节）
 * @param flags 环境标志位（LMJCORE_* 组合）
 * @param ptr_gen 指针生成器
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
 * @brief 开启事务
 * @param env 环境句柄
 * @param parent 父事务（可为 NULL）
 * @param flags 事务标志（LMJCORE_TXN_* 组合）
 * @param txn 输出事务句柄
 * @return 错误码
 *
 * @note 父子事务规则：
 *       - 写事务可开启任意子事务（读/写）
 *       - 只读事务不能开启任何子事务
 *       - parent = NULL 表示顶级事务
 */
int lmjcore_txn_begin(lmjcore_env *env, lmjcore_txn *parent, unsigned int flags,
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
 * @param result_buf 输出缓冲区
 * @param result_buf_size 输出缓冲区大小
 * @param result_head 输出参数，指向结果头部的指针
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_obj_get(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                    uint8_t *result_buf, size_t result_buf_size,
                    lmjcore_result_obj **result_head);

/**
 * @brief 完全删除对象成员（包括注册信息和值）
 *
 * 同时从 arr 库的成员列表和 main 库的键值对中删除该成员。
 * 注意：会先删除 main 中的值（如果存在），再删除 arr 中的注册。
 *
 * @param txn 有效的写事务句柄
 * @param obj_ptr 目标对象指针
 * @param member_name 成员名称（二进制安全字节序列）
 * @param member_name_len 成员名称长度
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_obj_del(lmjcore_txn *txn, const lmjcore_ptr obj_ptr);

/**
 * @brief 获取对象的成员列表
 *
 * 结果写入调用方提供的 result_buf 缓冲区，采用紧凑布局：
 *  [ lmjcore_result_arr头部 | element_descriptor数组 | 元素数据（从后向前） ]
 *
 * @param txn 有效事务句柄
 * @param obj_ptr 数组指针
 * @param result_buf 输出缓冲区
 * @param result_buf_size 缓冲区大小
 * @param result_head 输出：指向 result_buf 中 lmjcore_result_arr 结构的指针
 * @return 错误码（LMJCORE_SUCCESS 表示成功，即使有语义错误也视为成功）
 */
int lmjcore_obj_member_list(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                            uint8_t *result_buf, size_t result_buf_size,
                            lmjcore_result_arr **result_head);

/**
 * @brief 获取对象指定成员的值
 *
 * @param txn 有效的读事务句柄
 * @param obj_ptr 目标对象指针
 * @param member_name 成员名称（二进制安全字节序列）
 * @param member_name_len 成员名称长度
 * @param value_buf 输出缓冲区，用于接收成员值
 * @param value_buf_size 输出缓冲区大小
 * @param value_size_out 输出参数，返回实际值的长度
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_obj_member_get(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                           const uint8_t *member_name, size_t member_name_len,
                           uint8_t *value_buf, size_t value_buf_size,
                           size_t *value_size_out);

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

/**
 * @brief 统计数组的元素总长度和数量
 *
 * @param txn 有效的读事务句柄
 * @param obj_ptr 数组指针
 * @param total_member_len_out 输出参数，元素值总长度
 * @param member_count_out 输出参数，元素总数量
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_obj_stat_members(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                             size_t *total_member_len_out,
                             size_t *member_count_out);
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
 * @brief 获取数组的所有元素
 *
 * 结果写入调用方提供的 result_buf 缓冲区，采用紧凑布局：
 *  [ lmjcore_result_arr头部 | element_descriptor数组 | 元素数据（从后向前） ]
 *
 * @param txn 有效事务句柄
 * @param arr_ptr 数组指针
 * @param result_buf 输出缓冲区
 * @param result_buf_size 缓冲区大小
 * @param result_head 输出：指向 result_buf 中 lmjcore_result_arr 结构的指针
 * @return 错误码（LMJCORE_SUCCESS 表示成功，即使有语义错误也视为成功）
 */
int lmjcore_arr_get(lmjcore_txn *txn, const lmjcore_ptr arr_ptr,
                    uint8_t *result_buf, size_t result_buf_size,
                    lmjcore_result_arr **result_head);

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
 * @brief 统计数组的元素总长度和数量
 *
 * @param txn 有效的读事务句柄
 * @param arr_ptr 数组指针
 * @param total_value_len_out 输出参数，元素值总长度
 * @param element_count_out 输出参数，元素总数量
 * @return int 错误码（LMJCORE_SUCCESS 表示成功）
 */
int lmjcore_arr_stat_element(lmjcore_txn *txn, const lmjcore_ptr arr_ptr,
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

// ==================== 工具 ====================

/**
 * @brief 判断事务的类型
 *
 * @param txn 事务句柄
 * @return true 是只读类型
 * @return false 不是只读类型
 */
bool lmjcore_txn_is_read_only(lmjcore_txn *txn);

/**
 * @brief 将错误代码转换为字符串
 *
 * @param error_code 错误代码
 * @return const char*
 */
const char *lmjcore_strerror(int error_code);

#ifdef __cplusplus
}
#endif

#endif // LMJCORE_H