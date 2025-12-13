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

// LMDB 错误转换（复用你提供的逻辑）
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
pub const Ptr = [17]u8;

pub const EntityType = enum(u8) {
    obj = c.LMJCORE_OBJ,
    arr = c.LMJCORE_ARR,
};

pub const TxnType = enum {
    readonly,
    write,
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

// === 描述符结构（Zig 版本）===
pub const Descriptor = extern struct {
    value_offset: usize,
    value_len: usize,

    // 辅助方法：从 buffer 中提取值切片
    pub fn getValue(self: *const Descriptor, buffer: []const u8) []const u8 {
        return buffer[self.value_offset .. self.value_offset + self.value_len];
    }
};

pub const MemberDescriptor = extern struct {
    member_name: Descriptor,
    member_value: Descriptor,
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
    member_offset: usize,
    member_len: u16,
    auditError: AuditErrorCode,
};

pub const AuditReport = extern struct {
    audit_descriptor: [*]AuditDescriptor,
    audit_cont: usize,

    pub fn getDescriptors(self: *const AuditReport) []const AuditDescriptor {
        return self.audit_descriptor[0..self.audit_cont];
    }

    pub fn getMemberName(self: *const AuditDescriptor, buffer: []const u8) []const u8 {
        return buffer[self.member_offset .. self.member_offset + self.member_len];
    }
};

// === 函数指针类型 ===
pub const PtrGeneratorFn = *const fn (?*anyopaque, [*c]u8) callconv(.c) c_int;

pub const ResultObj = extern struct {
    error_count: usize,
    errors: [c.LMJCORE_MAX_READ_ERRORS]ReadError,
    member_count: usize,
    members: [0]MemberDescriptor, // flexible array

    pub fn getMembers(self: *const ResultObj) []const MemberDescriptor {
        return @as([*]const MemberDescriptor, @ptrCast(&self.members))[0..self.member_count];
    }

    pub fn getName(self: *const MemberDescriptor, buffer: []const u8) []const u8 {
        return self.member_name.getValue(buffer);
    }

    pub fn getValue(self: *const MemberDescriptor, buffer: []const u8) []const u8 {
        return self.member_value.getValue(buffer);
    }
};

pub const ResultArr = extern struct {
    error_count: usize,
    errors: [c.LMJCORE_MAX_READ_ERRORS]ReadError,
    element_count: usize,
    elements: [0]Descriptor,

    pub fn getElements(self: *const ResultArr) []const Descriptor {
        return @as([*]const Descriptor, @ptrCast(&self.elements))[0..self.element_count];
    }
};

// === 高级封装：安全读取对象 ===
pub fn readObject(
    txn: *Txn,
    obj_ptr: Ptr,
    allocator: std.mem.Allocator,
) !struct { result: *ResultObj, buffer: []u8 } {
    // 初次调用估算大小（可选：先 stat_values）
    var buf_size: usize = 4096;
    while (true) {
        const buffer = try allocator.alloc(u8, buf_size);
        var result_head: ?*c.lmjcore_result_obj = undefined;
        const rc = c.lmjcore_obj_get(txn, &obj_ptr, buffer.ptr, buffer.len, &result_head);
        if (rc == c.LMJCORE_ERROR_BUFFER_TOO_SMALL) {
            allocator.free(buffer);
            buf_size *= 2;
            continue;
        }
        try throw(rc);
        if (result_head == null) return error.UnexpectedNull;
        const zig_result = @as(*ResultObj, @ptrCast(result_head.?));
        return .{ .result = zig_result, .buffer = buffer };
    }
}

// === 高级封装：安全读取数组 ===
pub fn readArray(
    txn: *Txn,
    arr_ptr: Ptr,
    allocator: std.mem.Allocator,
) !struct { result: *ResultArr, buffer: []u8 } {
    var buf_size: usize = 4096;
    while (true) {
        const buffer = try allocator.alloc(u8, buf_size);
        var result_head: ?*c.lmjcore_result_arr = undefined;
        const rc = c.lmjcore_arr_get(txn, &arr_ptr, buffer.ptr, buffer.len, &result_head);
        if (rc == c.LMJCORE_ERROR_BUFFER_TOO_SMALL) {
            allocator.free(buffer);
            buf_size *= 2;
            continue;
        }
        try throw(rc);
        if (result_head == null) return error.UnexpectedNull;
        const zig_result = @as(*ResultArr, @ptrCast(result_head.?));
        return .{ .result = zig_result, .buffer = buffer };
    }
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
    const rc = c.lmjcore_init(path, map_size, flags, ptr_gen, ptr_gen_ctx, &env);
    try throw(rc);
    return @as(*Env, @ptrCast(env.?));
}

pub fn cleanup(env: *Env) !void {
    const rc = c.lmjcore_cleanup(@as(*c.lmjcore_env, @ptrCast(env)));
    try throw(rc);
}

pub fn txnBegin(env: *Env, typ: TxnType) !*Txn {
    const c_type = switch (typ) {
        .readonly => c.LMJCORE_TXN_READONLY,
        .write => c.LMJCORE_TXN_WRITE,
    };
    var txn: ?*c.lmjcore_txn = undefined;
    const rc = c.lmjcore_txn_begin(@as(*c.lmjcore_env, @ptrCast(env)), c_type, &txn);
    try throw(rc);
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
    const rc = c.lmjcore_obj_create(@as(*c.lmjcore_txn, @ptrCast(txn)), &ptr);
    try throw(rc);
    return ptr;
}

pub fn objRegister(txn: *Txn, ptr: Ptr) !void {
    const rc = c.lmjcore_obj_register(@as(*c.lmjcore_txn, @ptrCast(txn)), &ptr);
    try throw(rc);
}

// 成员操作
pub fn objMemberPut(
    txn: *Txn,
    obj_ptr: Ptr,
    name: []const u8,
    value: []const u8,
) !void {
    const rc = c.lmjcore_obj_member_put(
        @as(*c.lmjcore_txn, @ptrCast(txn)),
        &obj_ptr,
        name.ptr,
        name.len,
        value.ptr,
        value.len,
    );
    try throw(rc);
}

pub fn objMemberGet(
    txn: *Txn,
    obj_ptr: Ptr,
    name: []const u8,
    out_buf: []u8,
) !usize {
    var actual_len: usize = undefined;
    const rc = c.lmjcore_obj_member_get(
        @as(*c.lmjcore_txn, @ptrCast(txn)),
        &obj_ptr,
        name.ptr,
        name.len,
        out_buf.ptr,
        out_buf.len,
        &actual_len,
    );
    try throw(rc);
    return actual_len;
}

// 存在性检查
pub fn entityExist(txn: *Txn, ptr: Ptr) !bool {
    const rc = c.lmjcore_entity_exist(@as(*c.lmjcore_txn, @ptrCast(txn)), &ptr);
    if (rc < 0) try throw(rc);
    return rc == 1;
}

// 审计
pub fn auditObject(
    txn: *Txn,
    obj_ptr: Ptr,
    allocator: std.mem.Allocator,
) !struct { report: *AuditReport, buffer: []u8 } {
    var buf_size: usize = 4096;
    while (true) {
        const buffer = try allocator.alloc(u8, buf_size);
        var report_head: ?*c.lmjcore_audit_report = undefined;
        const rc = c.lmjcore_audit_object(@as(*c.lmjcore_txn, @ptrCast(txn)), &obj_ptr, buffer.ptr, buffer.len, &report_head);
        if (rc == c.LMJCORE_ERROR_BUFFER_TOO_SMALL) {
            allocator.free(buffer);
            buf_size *= 2;
            continue;
        }
        try throw(rc);
        if (report_head == null) return error.UnexpectedNull;
        const zig_report = @as(*AuditReport, @ptrCast(report_head.?));
        return .{ .report = zig_report, .buffer = buffer };
    }
}

pub fn repairObject(txn: *Txn, report: *AuditReport, buffer: []u8) !void {
    const rc = c.lmjcore_repair_object(
        @as(*c.lmjcore_txn, @ptrCast(txn)),
        buffer.ptr,
        buffer.len,
        @as(*c.lmjcore_audit_report, @ptrCast(report)),
    );
    try throw(rc);
}
