# LMJCore

**嵌入式存储内核 · 把嵌套数据“摊平”的高性能引擎**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Language](https://img.shields.io/badge/language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![LMDB](https://img.shields.io/badge/built%20on-LMDB-green.svg)](http://www.lmdb.tech/doc/)

---

## 📖 概述

LMJCore 是一个基于 [LMDB](http://www.lmdb.tech/doc/) 的嵌入式存储内核，专为**高度动态、深度嵌套的数据**（如 JSON、配置树、图结构）设计。它不提供查询语言，而是专注做一件事：

> **把复杂结构“拆开存”，用全局指针重新“拼起来”——实现极致读写性能与强一致性。**

### 核心思想（三句话讲清）

1. **万物皆指针**  
   每个对象/集合都有一个 **17 字节全局唯一 ID**（首字节标识类型：`0x01`=对象，`0x02`=集合）。

2. **数据被彻底扁平化**  
   嵌套结构不会以树形存储，而是拆成独立片段，分布在两个物理空间中。

3. **靠两个“仓库”协作**  
   - **`set`（集合区）**：回答“这个实体有哪些关联项？”（成员名列表、集合元素列表）  
   - **`main`（主存储区）**：回答“这个成员的具体值是多少？”（`name → "Alice"`）

---

## ✨ 特性

- **🚀 极致读性能**  
  成员可并发点查 `main`，无需递归解析嵌套结构。

- **📦 局部更新**  
  修改一个字段只需更新一个 `main` 格子，不影响父结构。

- **⚡ 继承 LMDB 优势**  
  ACID 事务、MVCC、零拷贝、内存映射、崩溃安全。

- **🔧 真正无模式**  
  Value 是纯二进制，上层可自由编码（JSON、Protobuf、自定义）。

- **🔗 无限可扩展**  
  通过指针轻松构建图、树、双向引用等复杂拓扑。

- **🛡️ 完整性保障**  
  提供审计机制检测“幽灵成员”，确保数据一致性。

- **🔢 集合语义**  
  集合是无序、自动去重的，符合数学集合概念；若需保序，可在 Value 中编码下标。

---

## 📦 安装

### 依赖
- [LMDB](https://github.com/LMDB/lmdb)（需预先安装）
- C99 编译器

### 编译

```bash
git clone https://github.com/QiDream-hub/LMJCore.git
cd lmjcore
make
```

### 集成到你的项目

只需包含 `lmjcore.h` 并链接 `liblmjcore.a` 和 LMDB：

```c
#include "lmjcore.h"
// 链接时添加 -llmjcore -llmdb
```

---

## 🚀 快速开始

### 1. 初始化环境

```c
lmjcore_env *env;
int rc = lmjcore_init("./data", 10485760, LMJCORE_ENV_SAFE, 
                      my_ptr_generator, NULL, &env);
if (rc != LMJCORE_SUCCESS) {
    printf("初始化失败: %s\n", lmjcore_strerror(rc));
    return -1;
}
```

### 2. 创建对象并写入数据

```c
lmjcore_txn *txn;
lmjcore_ptr obj_ptr;

// 开启写事务
lmjcore_txn_begin(env, NULL, 0, &txn);

// 创建对象
lmjcore_obj_create(txn, obj_ptr);

// 写入成员
lmjcore_obj_member_put(txn, obj_ptr, 
                       (uint8_t*)"name", 4,
                       (uint8_t*)"Alice", 5);

lmjcore_obj_member_put(txn, obj_ptr,
                       (uint8_t*)"age", 3,
                       (uint8_t*)"30", 2);

// 提交事务
lmjcore_txn_commit(txn);
```

### 3. 创建集合并添加元素

```c
lmjcore_ptr set_ptr;

lmjcore_txn_begin(env, NULL, 0, &txn);

// 创建集合
lmjcore_set_create(txn, set_ptr);

// 添加元素（自动去重、按字典序排序）
lmjcore_set_add(txn, set_ptr, (uint8_t*)"apple", 5);
lmjcore_set_add(txn, set_ptr, (uint8_t*)"banana", 6);
lmjcore_set_add(txn, set_ptr, (uint8_t*)"apple", 5); // 重复添加，返回错误

lmjcore_txn_commit(txn);
```

### 4. 读取对象

```c
uint8_t buf[4096];
lmjcore_result_obj *result;

lmjcore_txn_begin(env, NULL, LMJCORE_TXN_READONLY, &txn);

rc = lmjcore_obj_get(txn, obj_ptr, buf, sizeof(buf), &result);
if (rc == LMJCORE_SUCCESS) {
    printf("对象有 %zu 个成员\n", result->member_count);
    // 遍历成员...
}

lmjcore_txn_abort(txn);
```

### 5. 读取集合

```c
uint8_t set_buf[4096];
lmjcore_result_set *set_result;

lmjcore_txn_begin(env, NULL, LMJCORE_TXN_READONLY, &txn);

rc = lmjcore_set_get(txn, set_ptr, set_buf, sizeof(set_buf), &set_result);
if (rc == LMJCORE_SUCCESS) {
    printf("集合有 %zu 个元素\n", set_result->element_count);
    // 遍历元素（按字典序返回）...
}

lmjcore_txn_abort(txn);
```

### 6. 审计数据完整性

```c
uint8_t audit_buf[8192];
lmjcore_audit_report *report;

rc = lmjcore_audit_object(txn, obj_ptr, audit_buf, sizeof(audit_buf), &report);
if (report->audit_count > 0) {
    printf("发现 %zu 个幽灵成员，准备修复\n", report->audit_count);
    lmjcore_repair_object(txn, audit_buf, sizeof(audit_buf), report);
}
```

---

## 📚 核心概念

| 概念 | 说明 |
|------|------|
| **指针** | 17 字节全局唯一 ID，首字节标识类型（对象/集合） |
| **`set` 数据库** | 存储实体的关联项集合（成员名列表、集合元素列表），支持多值、自动去重、按字典序排序 |
| **`main` 数据库** | 存储具体值，Key = `[指针][成员名]`，点查 O(1) |
| **缺失值** | `set` 中有成员名但 `main` 中无值 → 合法中间状态 |
| **幽灵成员** | `main` 中有值但 `set` 中未注册 → 数据损坏 |

### 集合 vs 对象

| 维度 | 对象 | 集合 |
|------|------|------|
| **指针标识** | `0x01` | `0x02` |
| **结构** | 键值对（成员名 → 值） | 元素列表（无序） |
| **存储位置** | 成员名在 `set`，值在 `main` | 元素在 `set` |
| **访问方式** | 通过成员名点查 | 遍历或判断存在性 |
| **典型场景** | 配置、用户信息、文档 | 标签、权限组、唯一值列表 |

> ⚠️ **重要**：集合**不保留插入顺序**，按字典序自动排序。若需有序列表，请在 Value 中编码下标（如前 4 字节）。

想深入了解？请阅读：
- [LMJCore 核心存储模型](docs/LMJCore%20核心存储模型.md)
- [LMJCore 概念指南](docs/LMJCore%20概念指南.md)
- [LMJCore 核心设计定义](docs/LMJCore%20核心设计定义.md)
- [LMJCore 事务模型](docs/LMJCore%20事务模型.md)

---

## 🛠️ API 概览

### 环境管理
```c
int lmjcore_init(const char *path, size_t map_size, unsigned int flags,
                 lmjcore_ptr_generator_fn ptr_gen, void *ptr_gen_ctx,
                 lmjcore_env **env);
int lmjcore_cleanup(lmjcore_env *env);
```

### 事务
```c
int lmjcore_txn_begin(lmjcore_env *env, lmjcore_txn *parent, 
                      unsigned int flags, lmjcore_txn **txn);
int lmjcore_txn_commit(lmjcore_txn *txn);
int lmjcore_txn_abort(lmjcore_txn *txn);
bool lmjcore_txn_is_read_only(lmjcore_txn *txn);
```

### 对象操作
```c
int lmjcore_obj_create(lmjcore_txn *txn, lmjcore_ptr ptr_out);
int lmjcore_obj_register(lmjcore_txn *txn, const lmjcore_ptr ptr);
int lmjcore_obj_get(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                    uint8_t *result_buf, size_t result_buf_size,
                    lmjcore_result_obj **result_head);
int lmjcore_obj_del(lmjcore_txn *txn, const lmjcore_ptr obj_ptr);
int lmjcore_obj_member_put(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                           const uint8_t *member_name, size_t member_name_len,
                           const uint8_t *value, size_t value_len);
int lmjcore_obj_member_get(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                           const uint8_t *member_name, size_t member_name_len,
                           uint8_t *value_buf, size_t value_buf_size,
                           size_t *value_size_out);
int lmjcore_obj_member_register(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                                const uint8_t *member_name, size_t member_name_len);
int lmjcore_obj_member_value_del(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                                 const uint8_t *member_name, size_t member_name_len);
int lmjcore_obj_member_del(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                           const uint8_t *member_name, size_t member_name_len);
int lmjcore_obj_member_list(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                            uint8_t *result_buf, size_t result_buf_size,
                            lmjcore_result_set **result_head);
```

### 集合操作
```c
int lmjcore_set_create(lmjcore_txn *txn, lmjcore_ptr ptr_out);
int lmjcore_set_add(lmjcore_txn *txn, const lmjcore_ptr set_ptr,
                    const uint8_t *value, size_t value_len);
int lmjcore_set_get(lmjcore_txn *txn, const lmjcore_ptr set_ptr,
                    uint8_t *result_buf, size_t result_buf_size,
                    lmjcore_result_set **result_head);
int lmjcore_set_del(lmjcore_txn *txn, const lmjcore_ptr set_ptr);
int lmjcore_set_remove(lmjcore_txn *txn, const lmjcore_ptr set_ptr,
                       const uint8_t *element, size_t element_len);
int lmjcore_set_contains(lmjcore_txn *txn, const lmjcore_ptr set_ptr,
                         const uint8_t *element, size_t element_len);
```

### 审计与修复
```c
int lmjcore_audit_object(lmjcore_txn *txn, const lmjcore_ptr obj_ptr,
                         uint8_t *report_buf, size_t report_buf_size,
                         lmjcore_audit_report **report_head);
int lmjcore_repair_object(lmjcore_txn *txn, uint8_t *report_buf,
                          size_t report_buf_size, lmjcore_audit_report *report);
```

### 工具函数
```c
int lmjcore_ptr_to_string(const lmjcore_ptr ptr, char *str_buf, size_t buf_size);
int lmjcore_ptr_from_string(const char *str, lmjcore_ptr ptr_out);
int lmjcore_entity_exist(lmjcore_txn *txn, const lmjcore_ptr ptr);
const char *lmjcore_strerror(int error_code);
```

---

## ⚙️ 配置选项

### 环境标志

| 标志 | 说明 |
|------|------|
| `LMJCORE_ENV_NOSYNC` | 不强制同步到磁盘（提升性能，可能丢数据） |
| `LMJCORE_ENV_WRITEMAP` | 使用可写内存映射（大幅提升写入性能） |
| `LMJCORE_ENV_SAFE` | 安全模式（默认），每个事务强制刷盘 |
| `LMJCORE_ENV_MAX_PERF` | 最大性能配置（组合所有优化标志） |

### 事务标志

| 标志 | 说明 |
|------|------|
| `LMJCORE_TXN_READONLY` | 只读事务 |
| `LMJCORE_TXN_NOSYNC` | 事务级不强制同步 |
| `LMJCORE_TXN_HIGH_PERF` | 事务高性能模式 |

---

## 📊 性能贴士

- **读事务要短**：长读事务会阻止 LMDB 清理旧数据页，导致文件膨胀。
- **批量写入**：将多个写入操作合并到同一事务，大幅提升性能。
- **合理设置 map_size**：根据数据量预估，过小会导致 `MDB_MAP_RESIZED`。
- **成员名 ≤ 493 字节**：受 LMDB Key 长度限制（17B 指针 + 成员名 ≤ 511B）。
- **集合顺序**：`set` 不保插入序，若需有序列表，请在 Value 中编码下标（如前 4 字节）。
- **集合元素唯一性**：重复添加相同元素会返回 `LMJCORE_ERROR_MEMBER_EXISTS`。

---

## 🔮 未来愿景

LMJCore 的设计不仅是嵌入式场景的优化，更是通向分布式架构的密钥：

- **单用户单实例**：每个用户独占一个实例，实现物理隔离与线性扩展
- **分布式路由网络**：指针即地址，构建无限水平扩展的联邦架构
- **在线热扩展**：挂载即服务，动态负载均衡

---

## 🤝 贡献指南

欢迎贡献！请遵循以下步骤：

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交改动 (`git commit -m 'Add some amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 提交 Pull Request

### 开发规范

- 遵循 [LMJCore 核心设计定义](docs/LMJCore%20核心设计定义.md) 中的契约
- 保持 API 风格一致
- 新增功能需编写单元测试
- 更新相关文档

---

## 📄 许可证

[MIT License](LICENSE) © 2025 LMJCore Contributors

---

## 🙏 致谢

- [LMDB](http://www.lmdb.tech/doc/) - 提供坚实的高性能 KV 存储基石
- 所有贡献者和使用者

---

**LMJCore - 把复杂留给自己，把简单留给上层**