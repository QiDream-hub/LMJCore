// test_runner.zig
const std = @import("std");
const testing = std.testing;

// 导入你的绑定
pub const lmj = @import("../core/include/Bindings/zig/lmjcore.zig");

// 导入具体的测试文件
test "Bindings: Flags Mapping" {
    try @import("test_flags.zig").run();
}

// 主函数用于命令行运行
pub fn main() !void {
    try testing.runTests(&.{});
}
