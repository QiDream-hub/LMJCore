const std = @import("std");
const lmj = @import("lmjcore");
const c = @import("c.zig").c;

// 模拟指针生成器（测试用）
fn mockPtrGen(ctx: ?*anyopaque, out: [*c]u8) callconv(.c) c_int {
    _ = ctx;
    // 简单填充0 (实际由库内部处理，这里只是为了链接通过)
    @memset(out[0..lmj.PtrLen], 0);
    return c.LMJCORE_SUCCESS;
}

// ==================================================
// 测试 1: 验证标志位映射逻辑
// ==================================================
fn testFlagsMapping() !void {
    std.debug.print("\n=== 测试 1: 标志位映射验证 ===\n", .{});

    // --- 环境标志测试 ---
    {
        std.debug.print("测试环境标志...\n", .{});

        // 构造一个包含多种标志的实例
        const flags = lmj.EnvFlags{
            .nosubdir = true,
            .nosync = true,
            .writemap = true,
            .mapasync = true,
        };

        const expected = c.LMJCORE_ENV_NOSUBDIR |
            c.LMJCORE_ENV_NOSYNC |
            c.LMJCORE_ENV_WRITEMAP |
            c.LMJCORE_ENV_MAPASYNC;
        const actual = flags.toInt();

        if (actual == expected) {
            std.debug.print(" 环境标志映射成功: Expected=0x{x}, Got=0x{x}\n", .{ expected, actual });
        } else {
            return error.TestFailed;
        }

        // 验证 fromInt 的回环
        const roundTrip = lmj.EnvFlags.fromInt(expected);
        if (roundTrip.nosubdir and roundTrip.nosync and roundTrip.writemap and roundTrip.mapasync) {
            std.debug.print(" 环境标志 fromInt 回环验证通过\n", .{});
        } else {
            return error.TestFailed;
        }
    }

    // --- 事务标志测试 ---
    {
        std.debug.print("测试事务标志...\n", .{});

        const flags = lmj.TxnFlags{
            .readonly = true,
            .notls = true,
        };

        const expected = c.LMJCORE_TXN_READONLY | c.LMJCORE_TXN_NOTLS;
        const actual = flags.toInt();

        if (actual == expected) {
            std.debug.print(" 事务标志映射成功: Expected=0x{x}, Got=0x{x}\n", .{ expected, actual });
        } else {
            return error.TestFailed;
        }
    }
}

// ==================================================
// 测试 2: 核心环境与事务生命周期
// ==================================================
fn testLifecycle() !void {
    std.debug.print("\n=== 测试 2: 核心生命周期测试 ===\n", .{});

    var env: ?*lmj.Env = null;
    var txn: ?*lmj.Txn = null;

    // 1. 初始化环境 (使用安全模式)
    try lmj.init("test_data", 1024 * 1024, lmj.EnvPresets.SAFE, mockPtrGen, null, &env);
    if (env) |e| {
        std.debug.print(" 环境初始化成功\n", .{});

        // 2. 开启事务 (使用默认标志)
        try lmj.txnBegin(e, null, lmj.TxnPresets.DEFAULT, &txn);
        if (txn) |t| {
            std.debug.print(" 事务开启成功\n", .{});

            // 3. 提交事务
            try lmj.txnCommit(t);
            std.debug.print(" 事务提交成功\n", .{});
        }

        // 4. 清理环境
        try lmj.cleanup(e);
        std.debug.print(" 环境清理成功\n", .{});
    } else {
        return error.UnexpectedNull;
    }
}

// ==================================================
// 测试 3: 预设常量验证
// ==================================================
fn testPresets() !void {
    std.debug.print("\n=== 测试 3: 预设常量验证 ===\n", .{});

    // 验证 MAX_PERF 预设是否包含预期的标志
    const perfFlags = lmj.EnvPresets.MAX_PERF;
    const hasWritemap = perfFlags.writemap;
    const hasNosync = perfFlags.nosync;

    if (hasWritemap and hasNosync) {
        std.debug.print(" MAX_PERF 预设包含正确的开关\n", .{});
    } else {
        return error.TestFailed;
    }

    // 验证只读事务预设
    if (lmj.TxnPresets.READONLY.readonly) {
        std.debug.print(" READONLY 预设验证通过\n", .{});
    } else {
        return error.TestFailed;
    }
}

// ==================================================
// 主测试入口
// ==================================================
pub fn main() !void {
    std.debug.print("🚀 开始运行 LMJCore Zig 封装测试\n", .{});

    // 运行映射测试
    try testFlagsMapping();

    // 运行预设测试
    try testPresets();

    // 运行生命周期测试
    try testLifecycle();

    std.debug.print("\n🎉 所有测试通过!\n", .{});
}
