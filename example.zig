const std = @import("std");
const lmj = @import("lmjcore");

// ==========================================
// 指针生成器回调函数
// ==========================================
// LMJCore 需要一个函数来生成 ID (Ptr)。
// 使用随机数生成 UUID 格式的 ID
fn ptrGenerator(ctx: ?*anyopaque, out: [*c]u8) callconv(.c) c_int {
    _ = ctx;

    // 创建随机数生成器
    var prng = std.Random.DefaultPrng.init(@as(u64, @intCast(std.time.timestamp())));
    const random = prng.random();

    // 填充随机字节
    for (1..lmj.PtrLen) |i| {
        out[i] = random.int(u8);
    }

    return 0; // LMJCORE_SUCCESS
}

// ==========================================
// 主程序
// ==========================================
pub fn main() !void {
    var env: ?*lmj.Env = null;
    var txn: ?*lmj.Txn = null;

    std.debug.print("🚀 开始 LMJCore 简单示例\n", .{});

    // 1. 初始化环境
    // 使用 SAFE 预设，数据存储在 "./lmjcore_db" 目录
    try lmj.init(
        "./lmjcore_db", // 路径
        1024 * 1024, // 1MB 映射大小 (测试用，实际需更大)
        lmj.EnvPresets.SAFE, // 标志位
        ptrGenerator, // 指针生成器
        null, // 上下文
        &env, // 输出环境句柄
    );
    std.debug.print("✅ 环境初始化完成\n", .{});

    // 2. 开启写事务
    try lmj.txnBegin(env.?, null, lmj.TxnPresets.DEFAULT, &txn);
    std.debug.print("✅ 写事务开启\n", .{});

    // 3. 创建一个新对象 (实体)
    var newObjPtr: lmj.Ptr = undefined;
    try lmj.objCreate(txn.?, &newObjPtr);
    var ptrStr: [35]u8 align(@alignOf(usize)) = undefined;
    try lmj.ptrToString(&newObjPtr, &ptrStr);
    const ptrStrSpils = ptrStr[0..ptrStr.len];
    std.debug.print("✅ 对象创建成功, Ptr: {s}\n", .{ptrStrSpils});

    // 4. 给对象添加成员 (Key-Value)
    try lmj.objMemberPut(txn.?, &newObjPtr, "name", "Alice");
    try lmj.objMemberPut(txn.?, &newObjPtr, "age", "30");
    std.debug.print("✅ 成员 'name' 和 'age' 写入完成\n", .{});

    // 5. 提交事务 (保存更改)
    try lmj.txnCommit(txn.?);
    txn = null; // 提交后事务指针失效
    std.debug.print("✅ 事务提交成功\n", .{});

    // ==========================================
    // 6. 读取验证 (开启一个新的只读事务)
    // ==========================================
    try lmj.txnBegin(env.?, null, lmj.TxnPresets.READONLY, &txn);
    std.debug.print("✅ 读事务开启\n", .{});

    // 分配一个缓冲区用于存放读取结果
    // LMJCore 使用“零拷贝”设计，需要传入一个大缓冲区
    var buffer: [512]u8 align(@alignOf(usize)) = undefined; // 注意对齐

    // 尝试读取对象
    const result = try lmj.readObject(txn.?, &newObjPtr, &buffer);
    std.debug.print("✅ 读取对象成功, 包含成员数: {d}\n", .{result.member_count});

    // 遍历并打印成员 (需要传入 buffer 来解析实际数据)
    for (result.getMembers()) |member_desc| {
        const name = member_desc.getName(buffer[0..]);
        const value = member_desc.getValue(buffer[0..]);
        std.debug.print("   -> {s}: {s}\n", .{ name, value });
    }

    // 7. 结束读事务
    try lmj.txnCommit(txn.?);
    std.debug.print("✅ 读事务提交\n", .{});

    // 8. 清理环境
    try lmj.cleanup(env.?);
    std.debug.print("✅ 环境清理完成\n", .{});

    std.debug.print("\n🎉 示例程序运行结束\n", .{});
}
