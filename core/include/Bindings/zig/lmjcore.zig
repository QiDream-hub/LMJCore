const std = @import("std");

const c = @cImport({
    @cInclude("/home/archLinux/Code/project/LMJsystem/LMJCore/core/include/lmjcore.h");
});

// 错误码定义
// const ErrorCode = enum(i32) {
// SUCCESS = c.LMJCORE_SUCCESS,
// INVALID_PARAM = c.LMJCORE_ERROR_INVALID_PARAM, // 参数无效
// MEMBER_TOO_LONG = c.LMJCORE_ERROR_MEMBER_TOO_LONG, // 成员名长度超限
// ENTITY_NOT_FOUND = c.LMJCORE_ERROR_ENTITY_NOT_FOUND, // 实体不存在（无效指针）
// GHOST_MEMBER = c.LMJCORE_ERROR_GHOST_MEMBER, // 检测到幽灵成员
// INVALID_POINTER = c.LMJCORE_ERROR_INVALID_POINTER, // 指针格式错误
// BUFFER_TOO_SMALL = c.LMJCORE_ERROR_BUFFER_TOO_SMALL, // 输出缓冲区空间不足
// MEMORY_ALLOCATION_FAILED = c.LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED, // 内存分配失败
// STRICT_MODE_VIOLATION = c.LMJCORE_ERROR_STRICT_MODE_VIOLATION, // 严格模式校验失败
// MEMBER_NOT_FOUND = c.LMJCORE_ERROR_MEMBER_NOT_FOUND, // 成员值不存在(缺失值)
// };

const core = struct {
    pub const EntityType = enum(u8) { Object = c.LMJCORE_OBJ, Array = c.LMJCORE_ARR };

    pub const ReadMode = enum(i32) { Loose = c.LMJCORE_MODE_LOOSE, Strict = c.LMJCORE_MODE_STRICT };

    pub const MAX_KEY_LEN: i32 = c.LMJCORE_MAX_KEY_LEN;

    pub const PTR_LEN: i32 = c.LMJCORE_PTR_LEN;

    pub const MAX_MEMBER_LEN: i32 = c.LMJCORE_MAX_MEMBER_NAME_LEN;

    pub const MAX_READ_ERRORS: i32 = c.LMJCORE_MAX_READ_ERRORS;

    pub const result = struct {
        pub const ErrorCode = enum(i32) {
            NONE = c.LMJCORE_READERR_NONE,
            ENTITY_NOT_FOUND = c.LMJCORE_READERR_ENTITY_NOT_FOUND,
            MEMBER_MISSING = c.LMJCORE_READERR_MEMBER_MISSING,
            BUFFER_TOO_SMALL = c.LMJCORE_READERR_BUFFER_TOO_SMALL,
            LMDB_FAILED = c.LMJCORE_READERR_LMDB_FAILED,
            MEMORY_ALLOCATION_FAILED = c.LMJCORE_READERR_MEMORY_ALLOCATION_FAILED,
        };

        // C 错误结构的 Zig 表示
        pub const ReadError = extern struct {
            code: ErrorCode,
            context: extern union {
                element: extern struct {
                    element_offset: u16,
                    element_len: u16,
                },
                error_code: extern struct {
                    code: i32,
                },
            },
            entity_ptr: [PTR_LEN]u8,
        };

        // 与 C 完全兼容的描述符
        pub const ObjectDescriptor = extern struct {
            name_offset: u32,
            name_len: u16,
            value_offset: u32,
            value_len: u16,
        };

        pub const ArrayDescriptor = extern struct {
            value_offset: u32,
            value_len: u16,
        };

        // 与 C 完全兼容的结果结构
        pub const Result = extern struct {
            errors: [MAX_READ_ERRORS]ReadError, // 注意：这里是 ReadError 不是 ErrorCode
            error_count: u8,
            count: usize, // 不是 u256!
            descriptor: extern union {
                object_descriptors: [*]ObjectDescriptor, // 字段名必须与 C 一致
                array_descriptors: [*]ArrayDescriptor, // 字段名必须与 C 一致
            },
        };
    };

    pub const env = struct {
        pub const Env = *c.lmjcore_env;

        pub const Flags = struct {
            pub const FIXEDMAP: u32 = c.LMJCORE_FLAGS_FIXEDMAP;
            pub const NOSUBDIR: u32 = c.LMJCORE_FLAGS_NOSUBDIR;
            pub const NOSYNC: u32 = c.LMJCORE_FLAGS_NOSYNC;
            pub const RDONLY: u32 = c.LMJCORE_FLAGS_RDONLY;
            pub const NOMETASYNC: u32 = c.LMJCORE_FLAGS_NOMETASYNC;
            pub const WRITEMAP: u32 = c.LMJCORE_FLAGS_WRITEMAP;
            pub const MAPASYNC: u32 = c.LMJCORE_FLAGS_MAPASYNC;
            pub const NOTLS: u32 = c.LMJCORE_FLAGS_NOTLS;
            pub const NOLOCK: u32 = c.LMJCORE_FLAGS_NOLOCK;
            pub const NORDAHEAD: u32 = c.LMJCORE_FLAGS_NORDAHEAD;
            pub const NOMEMINIT: u32 = c.LMJCORE_FLAGS_NOMEMINIT;

            // 常用配置组合
            pub const SAFE_SYNC: u32 = c.LMJCORE_SAFE_SYNC;
            pub const MAX_PERF: u32 = c.LMJCORE_MAX_PERF;
            pub const READONLY_MODE: u32 = c.LMJCORE_READONLY_MODE;
            pub const NO_DISK_SYNC: u32 = c.LMJCORE_NO_DISK_SYNC;
        };

        pub fn init(path: [*:0]const u8, map_size: usize, lmjcore_env_flags: Flags, ptr_gen: ?*const fn (?*anyopaque, [*]u8) i8, ptr_gen_ctx: ?*anyopaque, _env: **Env) !i32 {
            return c.lmjcore_init(@as([*c]const u8, path), map_size, @intFromEnum(lmjcore_env_flags), ptr_gen, ptr_gen_ctx, _env);
        }

        pub fn cleanup(_env: *Env) i32 {
            return c.lmjcore_cleanup(_env);
        }
    };

    pub const txn = struct {
        pub const Txn = c.lmjcore_txn;

        pub const Type = enum(i32) { WRITE = c.LMJCORE_TXN_WRITE, READNOLY = c.LMJCORE_TXN_READONLY };

        pub fn begin(_env: *env.Env, txn_type: Type, _txn: **Txn) !i32 {
            return c.lmjcore_txn_begin(_env, @intFromEnum(txn_type), _txn);
        }

        pub fn commit(_txn: *Txn) !i32 {
            return c.lmjcore_txn_commit(_txn);
        }

        pub fn abort(_txn: *Txn) !i32 {
            return c.lmjcore_txn_abort(_txn);
        }
    };

    pub const ptr = struct {
        pub const Ptr = [PTR_LEN]u8;

        pub const PTR_HEX_STRING_LEN: i32 = PTR_LEN * 2;

        pub fn toString(_ptr: *const Ptr, string_out: [PTR_HEX_STRING_LEN]u8) !i32 {
            return c.lmjcore_ptr_to_string(_ptr, string_out, string_out.len);
        }

        pub fn fromString(string: [PTR_HEX_STRING_LEN]u8, _ptr_out: *Ptr) !i32 {
            return c.lmjcore_ptr_from_string(string, _ptr_out);
        }
    };

    pub const obj = struct {
        pub fn create(_txn: *txn.Txn, _ptr_out: *const ptr.Ptr) !void {
            return c.lmjcore_obj_create(_txn, _ptr_out);
        }

        pub fn register(_txn: *txn.Txn, _ptr: *const ptr.Ptr) !i32 {
            return c.lmjcore_obj_register(_txn, _ptr);
        }

        pub fn get(_txn: *txn.Txn, _ptr: *const ptr.Ptr, mode: ReadMode, result_buf: []u8, result_buf_size: u256, result_head: **result.Result) !i32 {
            return c.lmjcore_obj_get(_txn, _ptr, @intFromEnum(mode), result_buf, result_buf_size, result_head);
        }

        pub fn dele(_txn: txn.Txn, _ptr: *const ptr.Ptr) !i32 {
            return c.lmjcore_obj_del(_txn, _ptr);
        }
    };
};
