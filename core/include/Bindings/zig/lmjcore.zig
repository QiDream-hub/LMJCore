const std = @import("std");
const c = @import("c.zig").c;

// === 错误码 ===
pub const Error = error{
    // LMJCore 专属错误（-32000 ~ -32999）
    InvalidParam,
    MemberTooLong,
    InvalidPointer,
    BufferTooSmall,
    MemoryAllocationFailed,
    EntityNotFound,
    MemberNotFound,

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
fn mapLmjcoreError(rc: c_int) ?Error!void {
    return switch (rc) {
        c.LMJCORE_SUCCESS => {},
        c.LMJCORE_ERROR_INVALID_PARAM => Error.InvalidParam,
        c.LMJCORE_ERROR_MEMBER_TOO_LONG => Error.MemberTooLong,
        c.LMJCORE_ERROR_INVALID_POINTER => Error.InvalidPointer,
        c.LMJCORE_ERROR_BUFFER_TOO_SMALL => Error.BufferTooSmall,
        c.LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED => Error.MemoryAllocationFailed,
        c.LMJCORE_ERROR_ENTITY_NOT_FOUND => Error.EntityNotFound,
        c.LMJCORE_ERROR_MEMBER_NOT_FOUND => Error.MemberNotFound,
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

// === 类型定义 ===
pub const Ptr = [c.LMJCORE_PTR_LEN]u8;

pub const EntityType = enum(u8) {
    obj = c.LMJCORE_OBJ,
    arr = c.LMJCORE_ARR,
};

pub const TxnType = enum(c.lmjcore_txn_type) {
    readonly = c.LMJCORE_TXN_READONLY,
    write = c.LMJCORE_TXN_WRITE,
};

pub const ReadErrorCode = enum(c.lmjcore_read_error_code) {
    none = c.LMJCORE_READERR_NONE,
    entity_not_found = c.LMJCORE_READERR_ENTITY_NOT_FOUND,
    member_missing = c.LMJCORE_READERR_MEMBER_MISSING,
};

pub const AuditErrorCode = enum(c.lmjcore_audit_error_code) {
    ghost_object = c.LMJCORE_AUDITERR_GHOST_OBJECT,
    ghost_member = c.LMJCORE_AUDITERR_GHOST_MEMBER,
    missing_value = c.LMJCORE_AUDITERR_MISSING_VALUE,
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
    code: ReadErrorCode,
    element: extern struct {
        element_offset: usize,
        element_len: usize,
    },
    entity_ptr: Ptr,
};

pub const AuditDescriptor = extern struct {
    ptr: Ptr,
    auditError: AuditErrorCode,
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
    path: [:0]const u8,
    map_size: usize,
    flags: u32,
    ptr_gen: ?PtrGeneratorFn,
    ptr_gen_ctx: ?*anyopaque,
) !*Env {
    var env: ?*c.lmjcore_env = undefined;
    const rc = c.lmjcore_init(
        path.ptr,
        map_size,
        flags,
        ptr_gen,
        ptr_gen_ctx,
        &env,
    );
    try throw(rc);
    if (env == null) return error.UnexpectedNull;
    return @as(*Env, @ptrCast(env.?));
}

pub fn cleanup(env: *Env) !void {
    const rc = c.lmjcore_cleanup(@as(*c.lmjcore_env, @ptrCast(env)));
    try throw(rc);
}

pub fn txnBegin(env: *Env, typ: TxnType) !*Txn {
    const c_type = @intFromEnum(typ);
    var txn: ?*c.lmjcore_txn = undefined;
    const rc = c.lmjcore_txn_begin(
        @as(*c.lmjcore_env, @ptrCast(env)),
        c_type,
        &txn,
    );
    try throw(rc);
    if (txn == null) return error.UnexpectedNull;
    return @as(*Txn, @ptrCast(txn.?));
}

pub fn txnCommit(txn: *Txn) !void {
    const rc = c.lmjcore_txn_commit(@as(*c.lmjcore_txn, @ptrCast(txn)));
    try throw(rc);
}

pub fn txnAbort(txn: *Txn) void {
    _ = c.lmjcore_txn_abort(@as(*c.lmjcore_txn, @ptrCast(txn)));
}

// 对象创建
pub fn objCreate(txn: *Txn) !Ptr {
    var ptr: Ptr = undefined;
    const rc = c.lmjcore_obj_create(
        @as(*c.lmjcore_txn, @ptrCast(txn)),
        mutPtrToC(&ptr),
    );
    try throw(rc);
    return ptr;
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
pub fn ptrToString(ptr: *const Ptr, allocator: std.mem.Allocator) ![:0]const u8 {
    // 34字符HEX + 空终止符
    const str = try allocator.alloc(u8, 35);
    const rc = c.lmjcore_ptr_to_string(ptrToC(ptr), str.ptr, str.len);
    try throw(rc);
    return str[0..34 :0];
}

pub fn ptrFromString(str: [:0]const u8) !Ptr {
    var ptr: Ptr = undefined;
    const rc = c.lmjcore_ptr_from_string(str.ptr, mutPtrToC(&ptr));
    try throw(rc);
    return ptr;
}
