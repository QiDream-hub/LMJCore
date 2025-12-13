# LMJCore —— 为极致性能而生的嵌入式存储内核 🚀

LMJCore 是一个基于 **LMDB** 构建的高性能、嵌入式、无模式（Schema-less）键值存储内核。  
通过创新的 **“指针扁平化”架构**，将任意嵌套数据结构（如 JSON、图、配置树）拆解为高效、可验证的扁平键值对，实现接近内存的访问速度与强一致性保障。

> 💡 **定位说明**：LMJCore 不是完整数据库，而是一个**可嵌入的存储内核**，为上层查询引擎（如未来的 `LMJQuery`）提供坚实、快速、一致的数据基石。

---

## 🌟 核心特性

### 🚀 极致性能
- **指针扁平化存储**：复杂嵌套结构被“拍平”存储，消除递归解析开销。
- **内存级读取**：受益于 LMDB 内存映射，点查延迟极低。
- **细粒度写入**：局部更新仅操作最小数据单元，无需重写整个对象。

### 🔧 灵活架构
- **真正的无类型存储**：所有值均为二进制安全字节数组，支持任意编码（JSON、Protobuf、自定义等）。
- **存索分离设计**：
  - `main` 数据库存储具体值（命名格子区）
  - `arr` 数据库存储成员/元素列表（集合袋区）
- **可插拔指针生成**：支持 UUID、ULID、雪花算法、自增 ID 等策略（需保证唯一性）。

### 🛡️ 数据安全
- **完整 ACID 事务**：继承 LMDB 的事务特性，确保原子性与崩溃安全。
- **数据状态自描述**：读取结果包含完整错误列表，上层可精确判断数据可用性。
- **幽灵成员检测**：内置审计与修复机制，主动发现并清理数据损坏。

### 📚 简洁原语
- **二元归一模型**：仅用 **对象（Object）** 和 **数组（Array）** 两种结构建模任意复杂数据。
- **统一指针系统**：17 字节全局唯一指针（含 1B 类型前缀 + 16B 唯一 ID）作为数据访问的唯一凭证。

---

## 🏗️ 架构概述
```plaintext
应用层
    │
    ├── 查询层 (LMJQuery) - 画饼中
    │
    └── LMJCore 存储内核
        ├── main 数据库 (存储单一映射)
        └── arr 数据库 (存储列表映射)
            │
            └── LMDB (存储引擎)
```


> ✨ **心智模型**：
> - `arr` 是“集合袋”：给定指针，列出它“拥有什么”（无序集合）。
> - `main` 是“命名格子”：给定“谁的 + 叫什么”，直接拿到值（O(1) 点查）。
> - **指针是胶水**：将分散片段重新组合成完整逻辑实体。

---

## 🚀 快速开始

### 安装依赖

```bash
# Ubuntu/Debian
sudo apt-get install liblmdb-dev

# CentOS/RHEL
sudo yum install lmdb-devel

# ArchLinux
sudo pacman -S lmdb

# macOS
brew install lmdb
````

### 基础使用（C API）

```c
#include "lmjcore.h"

// 初始化环境（必须提供指针生成器）
lmjcore_env* env;
lmjcore_init("/data/lmjcore", 1024 * 1024 * 1024, 0, NULL, NULL, &env);

// 创建写事务
lmjcore_txn* txn;
lmjcore_txn_begin(env, LMJCORE_TXN_WRITE, &txn);

// 创建对象并写入数据
lmjcore_ptr obj_ptr;
lmjcore_obj_create(txn, obj_ptr);

const char* name = "张三";
int age = 25;
lmjcore_obj_member_put(txn, obj_ptr, (uint8_t*)"name", 4, (uint8_t*)name, strlen(name));
lmjcore_obj_member_put(txn, obj_ptr, (uint8_t*)"age", 3, (uint8_t*)&age, sizeof(age));

// 提交事务
lmjcore_txn_commit(txn);

// 读取数据（结果自包含完整性信息）
lmjcore_txn_begin(env, LMJCORE_TXN_READONLY, &txn);
uint8_t result_buf[8192];
lmjcore_result_obj* result;
int rc = lmjcore_obj_get(txn, obj_ptr, result_buf, sizeof(result_buf), &result);

if (rc == LMJCORE_SUCCESS) {
    // 检查是否有缺失成员或其他错误
    if (result->error_count > 0) {
        printf("警告：对象存在 %zu 个问题\n", result->error_count);
        // 可遍历 result->errors[] 查看具体错误
    }

    // 安全遍历有效成员
    for (size_t i = 0; i < result->member_count; i++) {
        lmjcore_member_descriptor* desc = &result->members[i];
        char* member_name = (char*)(result_buf + desc->member_name.value_offset);
        uint8_t* value = result_buf + desc->member_value.value_offset;
        printf("%.*s: %.*s\n",
               (int)desc->member_name.value_len, member_name,
               (int)desc->member_value.value_len, value);
    }
}
lmjcore_txn_commit(txn);
lmjcore_cleanup(env);
```

---

## 📖 核心概念

### 指针系统

- 固定 17 字节：`[类型:1B][唯一ID:16B]`
    - `0x01` → 对象（Object）
    - `0x02` → 数组（Array）
- 全局唯一，由上层生成器保证（内核不校验唯一性）。

### 数据可用性由返回体自描述

LMJCore **不提供“宽松/严格”读取模式开关**。  
所有读取操作返回的结构体（如 `lmjcore_result_obj`）同时包含：

- `members[]`：成功读取的成员描述符
- `errors[]`：缺失值、实体不存在等错误详情（最多记录 8 项）

上层应用应检查 `error_count` 来判断数据是否完整可用：

- **`error_count == 0`**：对象完整。
- **存在 `LMJCORE_READERR_MEMBER_MISSING`**：某些成员已注册但未赋值（合法中间状态，如分步初始化）。
- **存在其他错误**：数据不可用，需处理异常。

> ✅ 此设计将策略决策权交给上层，同时提供完整诊断信息，避免隐式行为。

### 嵌套结构示例

```json
{ "user": { "name": "Bob" }, "tags": ["a", "b"] }
```

存储时：

- `main[parent_ptr + "user"]` → `child_obj_ptr`（17B 指针）
- `main[parent_ptr + "tags"]` → `tags_arr_ptr`（17B 指针）
- 子对象/数组各自在 `arr`/`main` 中独立定义

---

## 🔧 API 概览

### 环境管理

- `lmjcore_init()` / `lmjcore_cleanup()`

### 事务控制

- `lmjcore_txn_begin()` / `lmjcore_txn_commit()` / `lmjcore_txn_abort()`

### 对象操作

- `lmjcore_obj_create()` / `lmjcore_obj_get()` / `lmjcore_obj_del()`
- `lmjcore_obj_member_put()` / `lmjcore_obj_member_get()`
- `lmjcore_obj_member_register()`（分步初始化）

### 数组操作

- `lmjcore_arr_create()` / `lmjcore_arr_append()` / `lmjcore_arr_get()`

### 工具函数

- `lmjcore_ptr_to_string()` / `lmjcore_ptr_from_string()`
- `lmjcore_entity_exist()` / `lmjcore_obj_member_value_exist()`
- `lmjcore_audit_object()` / `lmjcore_repair_object()`（数据完整性保障）

---

## 🎯 性能特点

|操作|复杂度|说明|
|---|---|---|
|对象获取|O(log n) + k·O(1)|`arr` 查成员列表 + `main` 并发点查|
|成员更新|O(1)|仅修改 `main` 中一个格子|
|数组追加|O(log n)|LMDB 插入复杂度|

> ✅ 所有写操作在单事务内原子完成，避免中间状态暴露。

---

## 🔍 适用场景

### ✅ 理想用例

- 高并发读取场景（配置中心、用户画像缓存）
- 灵活 Schema 需求（快速迭代业务）
- 嵌入式环境（IoT 设备、边缘计算）
- 作为底层存储引擎构建上层数据库

### ❌ 不适用场景

- 复杂关联查询（需上层构建索引）
- 大规模 OLAP 分析任务
- 强 Schema 约束或 SQL 兼容性要求

---

## 🛠️ 构建与测试

```bash
git clone https://github.com/your-username/lmjcore.git
cd lmjcore
make
```

### 测试

```bash
# lmjcore 核心测试

#基础读写测试
mkdir -p lmjcre_db/
./build/LMJCoreTest 



```

---

## 🤝 贡献指南

我们欢迎各种形式的贡献！

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

---

## 📄 许可证

本项目采用 **MIT 许可证** —— 查看 LICENSE 文件了解详情。

---

## 🙏 关于本项目

> **真诚透明**：作为新人开发者，这是我第一次在 GitHub 上传项目。在 LMJCore 的开发过程中，我大量使用了 AI 辅助进行代码实现、文档编写和架构设计。虽然核心设计理念是我的，但具体实现得到了 AI 的很大帮助。这是一个学习成长的过程，感谢理解！

### 🙏 致谢

- 基于 [LMDB](https://symas.com/lmdb/) 构建，继承其卓越的存储特性
- 受现JSON数据结构,与c语言指针启发

---

## 📚 文档资源

- 核心设计定义
- 事务控制规范
- 实现细则与集成指南
- 架构愿景
- 指针关系管理规范

---

**LMJCore —— 用最简模型，释放极致性能。** 🚀