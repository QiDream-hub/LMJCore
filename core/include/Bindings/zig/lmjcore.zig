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

pub const lmjcore = struct {
    pub const EntityType = enum(u8) {
        Object = c.LMJCORE_OBJ,
        Array = c.LMJCORE_ARR,
    };

    pub const ReadMode = enum(i32) {
        Loose = c.LMJCORE_MODE_LOOSE,
        Strict = c.LMJCORE_MODE_STRICT,
    };

    pub const MAX_KEY_LEN: i32 = c.LMJCORE_MAX_KEY_LEN;
    pub const PTR_LEN: i32 = c.LMJCORE_PTR_LEN;
    pub const MAX_MEMBER_LEN: i32 = c.LMJCORE_MAX_MEMBER_NAME_LEN;
    pub const MAX_READ_ERRORS: i32 = c.LMJCORE_MAX_READ_ERRORS;

    pub const result = struct {
        pub const ReadErrorCode = enum(c_int) {
            none = 0,
            entity_not_found = 1,
            member_missing,
            buffer_too_small,
            lmdb_failed,
            memory_allocation_failed,
        };

        pub const ReadError = extern struct {
            code: ReadErrorCode,
            context: extern union {
                element: extern struct {
                    element_offset: u16,
                    element_len: u16,
                },
                error_code: extern struct {
                    code: c_int,
                },
            },
            entity_ptr: [lmjcore.PTR_LEN]u8,
        };

        pub const ObjDescriptor = extern struct {
            name_offset: u32,
            name_len: u16,
            value_offset: u32,
            value_len: u16,
        };

        pub const ArrDescriptor = extern struct {
            value_offset: u32,
            value_len: u16,
        };

        pub const Result = extern struct {
            errors: [lmjcore.MAX_READ_ERRORS]ReadError,
            error_count: u8,
            count: usize,
            descriptor: extern union {
                object_descriptors: [*]ObjDescriptor,
                array_descriptors: [*]ArrDescriptor,
            },
        };
    };

    pub const audit = struct {
        pub const AuditErrorCode = enum(c_int) {
            ghost_object = -1,
            ghost_member = -2,
            missing_value = -3,
        };

        pub const AuditDescriptor = extern struct {
            ptr: ptr.Ptr,
            member_offset: usize,
            member_len: u16,
            @"error": AuditErrorCode,
        };

        pub const AuditReport = extern struct {
            audit_descriptor: [*]AuditDescriptor,
            audit_cont: usize,
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

            pub const SAFE_SYNC: u32 = c.LMJCORE_SAFE_SYNC;
            pub const MAX_PERF: u32 = c.LMJCORE_MAX_PERF;
            pub const READONLY_MODE: u32 = c.LMJCORE_READONLY_MODE;
            pub const NO_DISK_SYNC: u32 = c.LMJCORE_NO_DISK_SYNC;
        };

        pub fn init(
            path: [*:0]const u8,
            map_size: usize,
            flags: u32,
            ptr_gen: ?*const fn (?*anyopaque, [*]u8) callconv(.C) i32,
            ptr_gen_ctx: ?*anyopaque,
            env_out: **Env,
        ) !i32 {
            return c.lmjcore_init(@ptrCast(path), map_size, flags, ptr_gen, ptr_gen_ctx, env_out);
        }

        pub fn cleanup(_env: *Env) i32 {
            return c.lmjcore_cleanup(_env);
        }
    };

    pub const txn = struct {
        pub const Txn = *c.lmjcore_txn;

        pub const Type = enum(i32) {
            READONLY = c.LMJCORE_TXN_READONLY,
            WRITE = c.LMJCORE_TXN_WRITE,
        };

        pub fn begin(_env: *env.Env, txn_type: Type, txn_out: **Txn) !i32 {
            return c.lmjcore_txn_begin(_env, @intFromEnum(txn_type), txn_out);
        }

        pub fn commit(_txn: Txn) !i32 {
            return c.lmjcore_txn_commit(_txn);
        }

        pub fn abort(_txn: Txn) !i32 {
            return c.lmjcore_txn_abort(_txn);
        }
    };

    pub const ptr = struct {
        pub const Ptr = [PTR_LEN]u8;
        pub const PTR_HEX_STRING_LEN = PTR_LEN * 2; // e.g., 34 for 17-byte ptr

        pub fn toString(ptr_val: *const Ptr, str_out: []u8) !i32 {
            if (str_out.len < PTR_HEX_STRING_LEN)
                return c.LMJCORE_ERROR_BUFFER_TOO_SMALL;
            return c.lmjcore_ptr_to_string(ptr_val, str_out.ptr, str_out.len);
        }

        pub fn fromString(str: [*:0]const u8, ptr_out: *Ptr) !i32 {
            return c.lmjcore_ptr_from_string(str, ptr_out);
        }
    };

    pub const obj = struct {
        pub fn create(w_txn: *txn.Txn, ptr_out: *ptr.Ptr) !i32 {
            return c.lmjcore_obj_create(w_txn, ptr_out);
        }

        pub fn register(w_txn: *txn.Txn, obj_ptr: *const ptr.Ptr) !i32 {
            return c.lmjcore_obj_register(w_txn, obj_ptr);
        }

        pub fn get(
            r_txn: *txn.Txn,
            obj_ptr: *const ptr.Ptr,
            mode: ReadMode,
            result_buf: []u8,
            result_head: **result.Result,
        ) !i32 {
            return c.lmjcore_obj_get(r_txn, obj_ptr, @intFromEnum(mode), result_buf.ptr, result_buf.len, result_head);
        }

        pub fn del(w_txn: *txn.Txn, obj_ptr: *const ptr.Ptr) !i32 {
            return c.lmjcore_obj_del(w_txn, obj_ptr);
        }

        pub fn stat(
            r_txn: *txn.Txn,
            obj_ptr: *const ptr.Ptr,
            total_value_len_out: *usize,
            total_value_count_out: *usize,
        ) !i32 {
            return c.lmjcore_obj_stat_values(r_txn, obj_ptr, total_value_len_out, total_value_count_out);
        }

        pub const member = struct {
            pub fn put(
                w_txn: *txn.Txn,
                obj_ptr: *const ptr.Ptr,
                member_name: []const u8,
                value: []const u8,
            ) !i32 {
                return c.lmjcore_obj_member_put(w_txn, obj_ptr, member_name.ptr, member_name.len, value.ptr, value.len);
            }

            pub fn register(
                w_txn: *txn.Txn,
                obj_ptr: *const ptr.Ptr,
                member_name: []const u8,
            ) !i32 {
                return c.lmjcore_obj_member_register(w_txn, obj_ptr, member_name.ptr, member_name.len);
            }

            pub fn get(
                r_txn: *txn.Txn,
                obj_ptr: *const ptr.Ptr,
                member_name: []const u8,
                value_buf: []u8,
                value_size_out: *usize,
            ) !i32 {
                return c.lmjcore_obj_member_get(r_txn, obj_ptr, member_name.ptr, member_name.len, value_buf.ptr, value_buf.len, value_size_out);
            }

            pub fn del(
                w_txn: *txn.Txn,
                obj_ptr: *const ptr.Ptr,
                member_name: []const u8,
            ) !i32 {
                return c.lmjcore_obj_member_del(w_txn, obj_ptr, member_name.ptr, member_name.len);
            }

            pub fn value_del(
                w_txn: *txn.Txn,
                obj_ptr: *const ptr.Ptr,
                member_name: []const u8,
            ) !i32 {
                return c.lmjcore_obj_member_value_del(w_txn, obj_ptr, member_name.ptr, member_name.len);
            }

            pub fn value_exist(
                r_txn: *txn.Txn,
                obj_ptr: *const ptr.Ptr,
                member_name: []const u8,
            ) !i32 {
                return c.lmjcore_obj_member_value_exist(r_txn, obj_ptr, member_name.ptr, member_name.len);
            }
        };

        pub const audit = struct {
            pub fn run(
                r_txn: *txn.Txn,
                obj_ptr: *const ptr.Ptr,
                report_buf: []u8,
                report_head: **lmjcore.audit.AuditReport,
            ) !i32 {
                return c.lmjcore_audit_object(r_txn, obj_ptr, report_buf.ptr, report_buf.len, report_head);
            }

            pub fn repair(
                w_txn: *txn.Txn,
                report_buf: []u8,
                report: *lmjcore.audit.AuditReport,
            ) !i32 {
                return c.lmjcore_repair_object(w_txn, report_buf.ptr, report_buf.len, report);
            }
        };
    };

    pub const arr = struct {
        pub fn create(w_txn: *txn.Txn, ptr_out: *ptr.Ptr) !i32 {
            return c.lmjcore_arr_create(w_txn, ptr_out);
        }

        pub fn append(
            w_txn: *txn.Txn,
            arr_ptr: *const ptr.Ptr,
            value: []const u8,
        ) !i32 {
            return c.lmjcore_arr_append(w_txn, arr_ptr, value.ptr, value.len);
        }

        pub fn get(
            r_txn: *txn.Txn,
            arr_ptr: *const ptr.Ptr,
            mode: ReadMode,
            result_buf: []u8,
            result_head: **result.Result,
        ) !i32 {
            return c.lmjcore_arr_get(r_txn, arr_ptr, @intFromEnum(mode), result_buf.ptr, result_buf.len, result_head);
        }

        pub fn del(w_txn: *txn.Txn, arr_ptr: *const ptr.Ptr) !i32 {
            return c.lmjcore_arr_del(w_txn, arr_ptr);
        }

        pub fn element_del(
            w_txn: *txn.Txn,
            arr_ptr: *const ptr.Ptr,
            element: []const u8,
        ) !i32 {
            return c.lmjcore_arr_element_del(w_txn, arr_ptr, element.ptr, element.len);
        }

        pub fn stat(
            r_txn: *txn.Txn,
            arr_ptr: *const ptr.Ptr,
            total_value_len_out: *usize,
            element_count_out: *usize,
        ) !i32 {
            return c.lmjcore_arr_stat_element(r_txn, arr_ptr, total_value_len_out, element_count_out);
        }
    };
};
