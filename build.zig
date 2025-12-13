const std = @import("std");

// pub fn build(b: *std.Build) void {
//     const target = b.standardTargetOptions(.{});
//     const optimize = b.standardOptimizeOption(.{});

//     const liblmjcore = b.addLibrary(.{
//         .name = "lmjcore",
//         .linkage = .static,
//         .root_module = b.createModule(.{
//             .root_source_file = b.path("./core/include/Bindings/zig/lmjcore.zig"),
//             .target = target,
//             .optimize = optimize,
//         }),
//     });

//     b.installArtifact(liblmjcore);
// }

// pub fn build(b: *std.Build) void {
//     const target = b.standardTargetOptions(.{});
//     const optimize = b.standardOptimizeOption(.{});

//     const lmjcore_module = b.addModule("lmjcore", .{
//         .root_source_file = b.path("core/include/Bindings/zig/lmjcore.zig"),
//         .target = target,
//         .optimize = optimize,
//     });

//     // 添加C语言头文件 lmjcore.h
//     lmjcore_module.addIncludePath(b.path("core/include/lmjcore.h"));
//     // 编译C语言源文件 lmjcore.c
//     lmjcore_module.addCSourceFile(.{
//         .file = b.path("core/src/lmjcore.c"),
//         .flags = &.{},
//     });

//     // 添加lmjcore的lmdb依赖
//     lmjcore_module.linkSystemLibrary("lmdb", .{ .needed = false });

//     // 添加事例程序
//     const example = b.addExecutable(.{
//         .name = "lmcjore_example",
//         .root_module = b.createModule(.{
//             .root_source_file = b.path("example.zig"),
//             .target = target,
//             .optimize = optimize,
//         }),
//     });

//     example.linkLibC();
//     example.root_module.addImport("lmjcore", lmjcore_module);

//     const example_runner = b.addRunArtifact(example);
//     b.step("run", "Run examle").dependOn(&example_runner.step);
// }

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // 创建 lmcore 模块
    const lmcore_module = b.addModule("lmjcore", .{
        .root_source_file = b.path("core/include/Bindings/zig/lmjcore.zig"),
        .target = target,
        .optimize = optimize,
    });

    // 添加头文件包含路径
    lmcore_module.addIncludePath(b.path("core/include"));
    lmcore_module.addIncludePath(b.path(".")); // 当前目录

    // 编译 C 语言源文件
    lmcore_module.addCSourceFile(.{
        .file = b.path("core/src/lmjcore.c"),
        .flags = &.{
            "-I", "core/include",
        },
    });

    // 添加 lmdb 依赖
    lmcore_module.linkSystemLibrary("lmdb", .{ .needed = false });

    // 创建示例程序
    const example = b.addExecutable(.{
        .name = "lmcjore_example",
        .root_module = b.createModule(.{
            .root_source_file = b.path("example.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });

    example.linkLibC();
    example.root_module.addImport("lmjcore", lmcore_module);

    // 添加包含路径到示例模块（重要！）
    example.root_module.addIncludePath(b.path("./Toolkit/ptr_uuid_gen/include"));
    example.root_module.addIncludePath(b.path("."));

    // 运行步骤
    const example_run = b.addRunArtifact(example);
    const example_step = b.step("run", "Run example");
    example_step.dependOn(&example_run.step);

    // 安装步骤
    b.installArtifact(example);
}
