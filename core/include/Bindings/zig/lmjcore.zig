const std = @import("std");
const c = @import("c.zig").c;

// === 错误码 ===
pub const Error = error{
    // LMJCore 专属错误（-32000 ~ -32999）
    // 参数相关 (-32000 ~ -32019)
    InvalidParam, // -32000: 通用参数无效
    NullPointer, // -32001: 空指针
    MemberTooLong, // -32002: 成员名长度超限
    InvalidPointer, // -32003: 指针格式错误
    BufferTooSmall, // -32004: 输出缓冲区空间不足

    // 事务相关 (-32020 ~ -32039)
    ReadonlyTxn, // -32020: 在只读事务中执行写操作
    ReadonlyParent, // -32021: 父事务是只读，不能开子事务

    // 实体相关 (-32040 ~ -32059)
    EntityNotFound, // -32040: 实体不存在
    EntityExists, // -32041: 实体已存在
    EntityTypeMismatch, // -32042: 实体类型不匹配

    // 成员相关 (-32060 ~ -32079)
    MemberNotFound, // -32060: 成员不存在
    MemberExists, // -32061: 成员已存在
    MemberMissing, // -32062: 成员缺失值

    // 资源相关 (-32080 ~ -32099)
    MemoryAllocationFailed, // -32080: 内存分配失败

    // 审计相关 (-32100 ~ -32119)
    GhostMember, // -32100: 存在幽灵成员

    // LMDB 错误（透传）
    INVAL,
    ACCES,
    NOMEM,
    NOENT,
    AGAIN,
    NOSPC,
    BUSY,
    INTR,
    PIPE,
    IO,
    MDB_KEYEXIST,
    MDB_NOTFOUND,
    MDB_PAGE_NOTFOUND,
    MDB_CORRUPTED,
    MDB_PANIC,
    MDB_VERSION_MISMATCH,
    MDB_INVALID,
    MDB_MAP_FULL,
    MDB_DBS_FULL,
    MDB_READERS_FULL,
    MDB_TLS_FULL,
    MDB_TXN_FULL,
    MDB_CURSOR_FULL,
    MDB_PAGE_FULL,
    MDB_MAP_RESIZED,
    MDB_INCOMPATIBLE,
    MDB_BAD_RSLOT,
    MDB_BAD_TXN,
    MDB_BAD_VALSIZE,
    MDB_BAD_DBI,
    MDB_UNKNOWN_ERROR,

    // Zig 层错误
    UnexpectedNull,
};

// LMJCore 错误码映射
fn mapLmjcoreError(rc: c_int) ?Error {
    return switch (rc) {
        c.LMJCORE_SUCCESS => null,
        c.LMJCORE_ERROR_INVALID_PARAM => Error.InvalidParam,
        c.LMJCORE_ERROR_NULL_POINTER => Error.NullPointer,
        c.LMJCORE_ERROR_MEMBER_TOO_LONG => Error.MemberTooLong,
        c.LMJCORE_ERROR_INVALID_POINTER => Error.InvalidPointer,
        c.LMJCORE_ERROR_BUFFER_TOO_SMALL => Error.BufferTooSmall,
        c.LMJCORE_ERROR_READONLY_TXN => Error.ReadonlyTxn,
        c.LMJCORE_ERROR_READONLY_PARENT => Error.ReadonlyParent,
        c.LMJCORE_ERROR_ENTITY_NOT_FOUND => Error.EntityNotFound,
        c.LMJCORE_ERROR_ENTITY_EXISTS => Error.EntityExists,
        c.LMJCORE_ERROR_ENTITY_TYPE_MISMATCH => Error.EntityTypeMismatch,
        c.LMJCORE_ERROR_MEMBER_NOT_FOUND => Error.MemberNotFound,
        c.LMJCORE_ERROR_MEMBER_EXISTS => Error.MemberExists,
        c.LMJCORE_ERROR_MEMBER_MISSING => Error.MemberMissing,
        c.LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED => Error.MemoryAllocationFailed,
        c.LMJCORE_ERROR_GHOST_MEMBER => Error.GhostMember,
        else => null,
    };
}

// 全局错误转换函数（先试 LMJCore，再试 LMDB）
pub fn throw(rc: c_int) Error!void {
    if (mapLmjcoreError(rc)) |err| return err;
    // 否则尝试 LMDB 错误
    try lmdbThrow(rc);
}

// LMDB 错误转换
fn lmdbThrow(rc: c_int) Error!void {
    try switch (rc) {
        c.MDB_SUCCESS => {},
        c.MDB_KEYEXIST => Error.MDB_KEYEXIST,
        c.MDB_NOTFOUND => Error.MDB_NOTFOUND,
        c.MDB_PAGE_NOTFOUND => Error.MDB_PAGE_NOTFOUND,
        c.MDB_CORRUPTED => Error.MDB_CORRUPTED,
        c.MDB_PANIC => Error.MDB_PANIC,
        c.MDB_VERSION_MISMATCH => Error.MDB_VERSION_MISMATCH,
        c.MDB_INVALID => Error.MDB_INVALID,
        c.MDB_MAP_FULL => Error.MDB_MAP_FULL,
        c.MDB_DBS_FULL => Error.MDB_DBS_FULL,
        c.MDB_READERS_FULL => Error.MDB_READERS_FULL,
        c.MDB_TLS_FULL => Error.MDB_TLS_FULL,
        c.MDB_TXN_FULL => Error.MDB_TXN_FULL,
        c.MDB_CURSOR_FULL => Error.MDB_CURSOR_FULL,
        c.MDB_PAGE_FULL => Error.MDB_PAGE_FULL,
        c.MDB_MAP_RESIZED => Error.MDB_MAP_RESIZED,
        c.MDB_INCOMPATIBLE => Error.MDB_INCOMPATIBLE,
        c.MDB_BAD_RSLOT => Error.MDB_BAD_RSLOT,
        c.MDB_BAD_TXN => Error.MDB_BAD_TXN,
        c.MDB_BAD_VALSIZE => Error.MDB_BAD_VALSIZE,
        c.MDB_BAD_DBI => Error.MDB_BAD_DBI,

        @intFromEnum(std.posix.E.INVAL) => Error.INVAL,
        @intFromEnum(std.posix.E.ACCES) => Error.ACCES,
        @intFromEnum(std.posix.E.NOMEM) => Error.NOMEM,
        @intFromEnum(std.posix.E.NOENT) => Error.NOENT,
        @intFromEnum(std.posix.E.AGAIN) => Error.AGAIN,
        @intFromEnum(std.posix.E.NOSPC) => Error.NOSPC,
        @intFromEnum(std.posix.E.BUSY) => Error.BUSY,
        @intFromEnum(std.posix.E.INTR) => Error.INTR,
        @intFromEnum(std.posix.E.PIPE) => Error.PIPE,
        @intFromEnum(std.posix.E.IO) => Error.IO,

        else => Error.MDB_UNKNOWN_ERROR,
    };
}

// ==========================================
// 1. 环境标志 (EnvFlags) 绑定
// ==========================================
/// 对应 lmjcore_env 的 flags
/// 参考 lmjcore.h 中的 #define LMJCORE_ENV_*
pub const EnvFlags = struct {
    fixedmap: bool = false,
    nosubdir: bool = false,
    nosync: bool = false,
    readonly: bool = false, // MDB_RDONLY for env
    nometasync: bool = false,
    writemap: bool = false,
    mapasync: bool = false,
    notls: bool = false,
    nolock: bool = false,
    noreadhead: bool = false,
    nomeminit: bool = false,

    /// 将 Zig 结构体转换为 C 期望的整数 (OR 操作)
    pub fn toInt(self: EnvFlags) c_uint {
        var result: c_uint = 0;

        // 使用 lmjcore 映射后的宏进行位或
        if (self.fixedmap) result |= c.LMJCORE_ENV_FIXEDMAP;
        if (self.nosubdir) result |= c.LMJCORE_ENV_NOSUBDIR;
        if (self.nosync) result |= c.LMJCORE_ENV_NOSYNC;
        if (self.readonly) result |= c.LMJCORE_ENV_READONLY; // 注意：环境也有只读标志
        if (self.nometasync) result |= c.LMJCORE_ENV_NOMETASYNC;
        if (self.writemap) result |= c.LMJCORE_ENV_WRITEMAP;
        if (self.mapasync) result |= c.LMJCORE_ENV_MAPASYNC;
        if (self.notls) result |= c.LMJCORE_ENV_NOTLS;
        if (self.nolock) result |= c.LMJCORE_ENV_NOLOCK;
        if (self.noreadhead) result |= c.LMJCORE_ENV_NORDAHEAD;
        if (self.nomeminit) result |= c.LMJCORE_ENV_NOMEMINIT;

        return result;
    }

    /// 从 C 返回的整数解析回 Zig 结构体
    pub fn fromInt(value: c_uint) EnvFlags {
        return EnvFlags{
            .fixedmap = (value & c.LMJCORE_ENV_FIXEDMAP) != 0,
            .nosubdir = (value & c.LMJCORE_ENV_NOSUBDIR) != 0,
            .nosync = (value & c.LMJCORE_ENV_NOSYNC) != 0,
            .readonly = (value & c.LMJCORE_ENV_READONLY) != 0,
            .nometasync = (value & c.LMJCORE_ENV_NOMETASYNC) != 0,
            .writemap = (value & c.LMJCORE_ENV_WRITEMAP) != 0,
            .mapasync = (value & c.LMJCORE_ENV_MAPASYNC) != 0,
            .notls = (value & c.LMJCORE_ENV_NOTLS) != 0,
            .nolock = (value & c.LMJCORE_ENV_NOLOCK) != 0,
            .noreadhead = (value & c.LMJCORE_ENV_NORDAHEAD) != 0,
            .nomeminit = (value & c.LMJCORE_ENV_NOMEMINIT) != 0,
        };
    }
};

// ==========================================
// 2. 事务标志 (TxnFlags) 绑定
// ==========================================
/// 对应 lmjcore_txn_begin 的 flags
/// 参考 lmjcore.h 中的 #define LMJCORE_TXN_*
pub const TxnFlags = struct {
    readonly: bool = false,
    nosync: bool = false,
    nometasync: bool = false,
    mapasync: bool = false,
    notls: bool = false,

    /// 将 Zig 结构体转换为 C 期望的整数
    pub fn toInt(self: TxnFlags) c_uint {
        var result: c_uint = 0;

        // 使用 lmjcore 映射后的宏进行位或
        if (self.readonly) result |= c.LMJCORE_TXN_READONLY;
        if (self.nosync) result |= c.LMJCORE_TXN_NOSYNC;
        if (self.nometasync) result |= c.LMJCORE_TXN_NOMETASYNC;
        if (self.mapasync) result |= c.LMJCORE_TXN_MAPASYNC;
        if (self.notls) result |= c.LMJCORE_TXN_NOTLS;

        return result;
    }

    /// 从 C 返回的整数解析回 Zig 结构体
    pub fn fromInt(value: c_uint) TxnFlags {
        return TxnFlags{
            .readonly = (value & c.LMJCORE_TXN_READONLY) != 0,
            .nosync = (value & c.LMJCORE_TXN_NOSYNC) != 0,
            .nometasync = (value & c.LMJCORE_TXN_NOMETASYNC) != 0,
            .mapasync = (value & c.LMJCORE_TXN_MAPASYNC) != 0,
            .notls = (value & c.LMJCORE_TXN_NOTLS) != 0,
        };
    }
};

// ==========================================
// 3. 便捷常量 (Presets)
// ==========================================
/// 预设的常用组合，直接对应 lmjcore.h 中的宏定义
pub const EnvPresets = struct {
    /// 最大性能配置 (WRITEMAP | MAPASYNC | NOSYNC | NOMETASYNC)
    pub const MAX_PERF = EnvFlags{
        .writemap = true,
        .mapasync = true,
        .nosync = true,
        .nometasync = true,
    };

    /// 安全模式 (0)
    pub const SAFE = EnvFlags{};

    /// 无锁模式
    pub const NOLOCK = EnvFlags{ .nolock = true };
};

pub const TxnPresets = struct {
    /// 默认事务 (0)
    pub const DEFAULT = TxnFlags{};

    /// 高性能事务 (不强制刷盘)
    pub const HIGH_PERF = TxnFlags{
        .nosync = true,
        .nometasync = true,
    };

    /// 只读事务
    pub const READONLY = TxnFlags{ .readonly = true };
};

// === 类型定义 ===
pub const Ptr = [c.LMJCORE_PTR_LEN]u8;
pub const PtrLen = c.LMJCORE_PTR_LEN;
pub const PtrSteingLen = c.LMJCORE_PTR_STRING_LEN;

pub const EntityType = enum(u8) {
    obj = c.LMJCORE_OBJ,
    arr = c.LMJCORE_ARR,
};

// === 句柄类型（opaque）===
pub const Env = opaque {};
pub const Txn = opaque {};

// === 描述符结构（与C内存布局完全一致）===
pub const Descriptor = extern struct {
    value_offset: usize,
    value_len: usize,

    // 从buffer中提取值切片（自动处理偏移量）
    pub fn getValue(self: *const Descriptor, buffer: []const u8) []const u8 {
        const start = self.value_offset;
        const end = start + self.value_len;
        // 确保偏移量在buffer范围内
        std.debug.assert(end <= buffer.len);
        return buffer[start..end];
    }
};

pub const MemberDescriptor = extern struct {
    member_name: Descriptor,
    member_value: Descriptor,

    // 从buffer中获取成员名
    pub fn getName(self: *const MemberDescriptor, buffer: []const u8) []const u8 {
        return self.member_name.getValue(buffer);
    }

    // 从buffer中获取成员值
    pub fn getValue(self: *const MemberDescriptor, buffer: []const u8) []const u8 {
        return self.member_value.getValue(buffer);
    }
};

pub const ReadError = extern struct {
    code: c_int,
    element: extern struct {
        element_offset: usize,
        element_len: usize,
    },
    entity_ptr: Ptr,

    pub fn getError(self: ReadError) !Error {
        return throw(self.code);
    }
};

pub const AuditDescriptor = extern struct {
    ptr: Ptr,
    member: MemberDescriptor,

    // 获取成员描述符切片
    pub fn getMembers(self: *const ResultObj) []const MemberDescriptor {
        // 直接通过指针转换获取柔性数组
        return @as([*]const MemberDescriptor, @ptrCast(&self.members))[0..self.member_count];
    }
};

// === 结果结构体定义（必须与C结构体完全匹配）===

// 对象返回体
pub const ResultObj = extern struct {
    error_count: usize,
    errors: [c.LMJCORE_MAX_READ_ERRORS]ReadError,
    member_count: usize,
    members: [0]MemberDescriptor, // 柔性数组

    // 获取成员描述符切片
    pub fn getMembers(self: *const ResultObj) []const MemberDescriptor {
        // 直接通过指针转换获取柔性数组
        return @as([*]const MemberDescriptor, @ptrCast(&self.members))[0..self.member_count];
    }

    // 遍历所有成员并打印（调试用）
    pub fn debugPrint(self: *const ResultObj, buffer: []const u8) void {
        std.debug.print("Object Result:\n", .{});
        std.debug.print("  member_count: {d}\n", .{self.member_count});
        std.debug.print("  error_count: {d}\n", .{self.error_count});

        if (self.error_count > 0) {
            for (self.errors[0..self.error_count]) |err| {
                std.debug.print("  Error: code={}, element_offset={d}, element_len={d}\n", .{
                    err.code,
                    err.element.element_offset,
                    err.element.element_len,
                });
            }
        }

        const members = self.getMembers();
        for (members, 0..) |member, i| {
            const name = member.getName(buffer);
            const value = member.getValue(buffer);
            std.debug.print("  Member[{d}]: name=\"{s}\" value=\"{s}\"\n", .{
                i,
                name,
                value,
            });
        }
    }
};

// 数组返回体
pub const ResultArr = extern struct {
    error_count: usize,
    errors: [c.LMJCORE_MAX_READ_ERRORS]ReadError,
    element_count: usize,
    elements: [0]Descriptor, // 柔性数组

    // 获取元素描述符切片
    pub fn getElements(self: *const ResultArr) []const Descriptor {
        return @as([*]const Descriptor, @ptrCast(&self.elements))[0..self.element_count];
    }

    // 遍历所有元素并打印（调试用）
    pub fn debugPrint(self: *const ResultArr, buffer: []const u8) void {
        std.debug.print("Array Result:\n", .{});
        std.debug.print("  element_count: {d}\n", .{self.element_count});
        std.debug.print("  error_count: {d}\n", .{self.error_count});

        if (self.error_count > 0) {
            for (self.errors[0..self.error_count]) |err| {
                std.debug.print("  Error: code={}, element_offset={d}, element_len={d}\n", .{
                    err.code,
                    err.element.element_offset,
                    err.element.element_len,
                });
            }
        }

        const elements = self.getElements();
        for (elements, 0..) |element, i| {
            const value = element.getValue(buffer);
            std.debug.print("  Element[{d}]: value=\"{s}\"\n", .{ i, value });
        }
    }
};

// 审计报告
pub const AuditReport = extern struct {
    audit_cont: usize,
    audit_descriptor: [0]AuditDescriptor,

    pub fn getDescriptors(self: *const AuditReport) []const AuditDescriptor {
        return @as([*]const AuditDescriptor, @ptrCast(&self.audit_descriptor))[0..self.audit_cont];
    }

    pub fn debugPrint(self: *const AuditReport) void {
        std.debug.print("Audit Report:\n", .{});
        std.debug.print("  audit_cont: {d}\n", .{self.audit_cont});
    }
};

// === 函数指针类型 ===
pub const PtrGeneratorFn = *const fn (?*anyopaque, [*c]u8) callconv(.c) c_int;

// === 指针辅助函数 ===
// 将Zig数组指针转换为C数组指针
fn ptrToC(ptr: *const Ptr) [*c]const u8 {
    return @as([*c]const u8, @ptrCast(ptr));
}

fn mutPtrToC(ptr: *Ptr) [*c]u8 {
    return @as([*c]u8, @ptrCast(ptr));
}

// === 读取对象 ===
pub fn readObject(
    txn: *Txn,
    obj_ptr: *const Ptr,
    buffer: []align(@sizeOf(usize)) u8,
) !*ResultObj {
    var result_head: ?*c.lmjcore_result_obj = undefined;
    const rc = c.lmjcore_obj_get(
        @as(*c.lmjcore_txn, @ptrCast(txn)),
        ptrToC(obj_ptr),
        buffer.ptr,
        buffer.len,
        &result_head,
    );

    try throw(rc);

    if (result_head == null) return error.UnexpectedNull;

    return @as(*ResultObj, @ptrCast(result_head.?));
}

// === 读取成员 ===
pub fn readMembers(
    txn: *Txn,
    obj_ptr: *const Ptr,
    buffer: []align(@sizeOf(usize)) u8,
) !*ResultArr {
    var result_head: ?*c.lmjcore_result_obj = undefined;
    const rc = c.lmjcore_obj_member_list(
        @as(*c.lmjcore_txn, @ptrCast(txn)),
        ptrToC(obj_ptr),
        buffer.ptr,
        buffer.len,
        &result_head,
    );

    try throw(rc);

    if (result_head == null) return error.UnexpectedNull;

    return @as(*ResultArr, @ptrCast(result_head.?));
}

// === 读取数组 ===
pub fn readArray(
    txn: *Txn,
    arr_ptr: *const Ptr,
    buffer: []align(@sizeOf(usize)) u8,
) !*ResultArr {
    var result_head: ?*c.lmjcore_result_obj = undefined;
    const rc = c.lmjcore_arr_get(
        @as(*c.lmjcore_txn, @ptrCast(txn)),
        ptrToC(arr_ptr),
        buffer.ptr,
        buffer.len,
        &result_head,
    );

    try throw(rc);

    if (result_head == null) return error.UnexpectedNull;

    return @as(*ResultArr, @ptrCast(result_head.?));
}

// === 审计对象封装 ===
pub fn auditObject(
    txn: *Txn,
    obj_ptr: *const Ptr,
    buffer: []align(@sizeOf(usize)) u8,
) !*AuditReport {
    var report_head: ?*c.lmjcore_audit_report = undefined;
    const rc = c.lmjcore_audit_object(
        @as(*c.lmjcore_txn, @ptrCast(txn)),
        ptrToC(obj_ptr),
        buffer.ptr,
        buffer.len,
        &report_head,
    );

    try throw(rc);

    if (report_head == null) return error.UnexpectedNull;

    return @as(*AuditReport, @ptrCast(report_head.?));
}

// === 核心 API 封装 ===
pub fn init(
    path: []const u8,
    map_size: usize,
    flags: EnvFlags,
    ptr_gen: PtrGeneratorFn,
    ptr_gen_ctx: ?*anyopaque,
    env: *?*Env,
) !void {
    const rc = c.lmjcore_init(
        path.ptr,
        map_size,
        flags.toInt(),
        ptr_gen,
        ptr_gen_ctx,
        env,
    );
    try throw(rc);
    if (env.* == null) return error.UnexpectedNull;
}

pub fn cleanup(env: *Env) !void {
    const rc = c.lmjcore_cleanup(@as(*c.lmjcore_env, @ptrCast(env)));
    try throw(rc);
}

pub fn txnBegin(env: *Env, parent: ?*Txn, flags: TxnFlags, txn: *?*Txn) !void {
    const rc = c.lmjcore_txn_begin(
        @as(*c.lmjcore_env, @ptrCast(env)),
        @as(?*c.lmjcore_txn, @ptrCast(parent)),
        flags.toInt(),
        @as(*?*c.lmjcore_txn, @ptrCast(txn)),
    );
    try throw(rc);
    if (txn.* == null) return error.UnexpectedNull;
}

pub fn txnCommit(txn: *Txn) !void {
    const rc = c.lmjcore_txn_commit(@as(*c.lmjcore_txn, @ptrCast(txn)));
    try throw(rc);
}

pub fn txnAbort(txn: *Txn) void {
    _ = c.lmjcore_txn_abort(@as(*c.lmjcore_txn, @ptrCast(txn)));
}

// 对象创建
pub fn objCreate(txn: *Txn, ptr: *Ptr) !void {
    const rc = c.lmjcore_obj_create(
        @as(*c.lmjcore_txn, @ptrCast(txn)),
        mutPtrToC(ptr),
    );
    try throw(rc);
}

pub fn objRegister(txn: *Txn, ptr: *const Ptr) !void {
    const rc = c.lmjcore_obj_register(
        @as(*c.lmjcore_txn, @ptrCast(txn)),
        ptrToC(ptr),
    );
    try throw(rc);
}

// 成员操作
pub fn objMemberPut(
    txn: *Txn,
    obj_ptr: *const Ptr,
    name: []const u8,
    value: []const u8,
) !void {
    const rc = c.lmjcore_obj_member_put(
        @as(*c.lmjcore_txn, @ptrCast(txn)),
        ptrToC(obj_ptr),
        name.ptr,
        name.len,
        value.ptr,
        value.len,
    );
    try throw(rc);
}

pub fn objMemberGet(
    txn: *Txn,
    obj_ptr: *const Ptr,
    name: []const u8,
    out_buf: []u8,
) !usize {
    var actual_len: usize = undefined;
    const rc = c.lmjcore_obj_member_get(
        @as(*c.lmjcore_txn, @ptrCast(txn)),
        ptrToC(obj_ptr),
        name.ptr,
        name.len,
        out_buf.ptr,
        out_buf.len,
        &actual_len,
    );
    try throw(rc);
    return actual_len;
}

// 数组操作
pub fn arrCreate(txn: *Txn) !Ptr {
    var ptr: Ptr = undefined;
    const rc = c.lmjcore_arr_create(
        @as(*c.lmjcore_txn, @ptrCast(txn)),
        mutPtrToC(&ptr),
    );
    try throw(rc);
    return ptr;
}

pub fn arrAppend(
    txn: *Txn,
    arr_ptr: *const Ptr,
    value: []const u8,
) !void {
    const rc = c.lmjcore_arr_append(
        @as(*c.lmjcore_txn, @ptrCast(txn)),
        ptrToC(arr_ptr),
        value.ptr,
        value.len,
    );
    try throw(rc);
}

// 存在性检查
pub fn entityExist(txn: *Txn, ptr: *const Ptr) !bool {
    const rc = c.lmjcore_entity_exist(
        @as(*c.lmjcore_txn, @ptrCast(txn)),
        ptrToC(ptr),
    );
    if (rc < 0) try throw(rc);
    return rc == 1;
}

// 修复对象--删除错误的成员
pub fn repairObject(txn: *Txn, report: *AuditReport, buffer: []u8) !void {
    const rc = c.lmjcore_repair_object(
        @as(*c.lmjcore_txn, @ptrCast(txn)),
        buffer.ptr,
        buffer.len,
        @as(*c.lmjcore_audit_report, @ptrCast(report)),
    );
    try throw(rc);
}

// 统计函数
pub fn objStatValues(
    txn: *Txn,
    obj_ptr: *const Ptr,
) !struct { total_value_len: usize, total_value_count: usize } {
    var total_len: usize = undefined;
    var total_count: usize = undefined;
    const rc = c.lmjcore_obj_stat_values(
        @as(*c.lmjcore_txn, @ptrCast(txn)),
        ptrToC(obj_ptr),
        &total_len,
        &total_count,
    );
    try throw(rc);
    return .{
        .total_value_len = total_len,
        .total_value_count = total_count,
    };
}

// 统计成员名长度
pub fn objStatMebers(
    txn: *Txn,
    obj_ptr: *const Ptr,
) !struct { total_value_len: usize, total_value_count: usize } {
    var total_len: usize = undefined;
    var total_count: usize = undefined;
    const rc = c.lmjcore_obj_stat_members(
        @as(*c.lmjcore_txn, @ptrCast(txn)),
        ptrToC(obj_ptr),
        &total_len,
        &total_count,
    );
    try throw(rc);
    return .{
        .total_value_len = total_len,
        .total_value_count = total_count,
    };
}

// 指针转换工具
pub fn ptrToString(
    ptr: *const Ptr,
    buffer: []align(@sizeOf(usize)) u8,
) !void {
    // 34字符HEX + 空终止符
    const rc = c.lmjcore_ptr_to_string(ptrToC(ptr), buffer.ptr, buffer.len);
    try throw(rc);
}

pub fn ptrFromString(str: *[:0]align(usize) const u8) !Ptr {
    var ptr: Ptr = undefined;
    const rc = c.lmjcore_ptr_from_string(str.ptr, mutPtrToC(&ptr));
    try throw(rc);
    return ptr;
}
