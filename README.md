# LMJCore


**LMJCore** 是一个基于 LMDB 的高性能、嵌入式、无模式（Schema-less）键值存储内核，通过创新的"指针扁平化"架构实现极致的数据访问性能。

## 🌟 核心特性

### 🚀 极致性能
- **指针扁平化存储**：将复杂嵌套数据结构转换为扁平键值对，消除递归解析开销
- **内存级读取**：受益于 LMDB 内存映射，实现接近内存的访问速度
- **细粒度写入**：局部更新只需操作最小数据单元，无需读写整个结构

### 🔧 灵活架构
- **真正的无类型存储**：所有值均为二进制安全字节数组，支持任意数据格式
- **存索分离设计**：`main` 数据库存储值，`arr` 数据库存储关系，职责清晰
- **可插拔指针生成**：支持 UUID、ULID、雪花算法等多种指针生成策略

### 🛡️ 数据安全
- **完整 ACID 事务**：继承 LMDB 的事务特性，保证数据一致性
- **双模式访问**：宽松模式保证业务连续性，严格模式确保数据完整性
- **幽灵成员检测**：内置数据完整性审计和修复机制

### 📚 简洁原语
- **二元归一模型**：仅用**对象 (Object)** 和**数组 (Array)** 两种结构建模复杂数据
- **统一指针系统**：17字节全局唯一指针作为数据访问的唯一凭证

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

## 🚀 快速开始

### 安装依赖

```bash
# Ubuntu/Debian
sudo apt-get install liblmdb-dev

# CentOS/RHEL
sudo yum install lmdb-devel

# macOS
brew install lmdb
```

### 基础使用

```c
#include "lmjcore.h"

// 初始化环境
lmjcore_env* env;
lmjcore_init("/data/lmjcore", 1024 * 1024 * 1024, NULL, NULL, &env);

// 创建写事务
lmjcore_txn* txn;
lmjcore_txn_begin(env, LMJCORE_TXN_WRITE, &txn);

// 创建对象并写入数据
lmjcore_ptr obj_ptr;
lmjcore_obj_create(txn, &obj_ptr);

const char* name = "张三";
int age = 25;
lmjcore_obj_put(txn, &obj_ptr, (uint8_t*)"name", 4, (uint8_t*)name, strlen(name));
lmjcore_obj_put(txn, &obj_ptr, (uint8_t*)"age", 3, (uint8_t*)&age, sizeof(age));

// 提交事务
lmjcore_txn_commit(txn);

// 读取数据
lmjcore_txn_begin(env, LMJCORE_TXN_READONLY, &txn);

uint8_t result_buf[8192];
lmjcore_result* result;
int rc = lmjcore_obj_get(txn, &obj_ptr, LMJCORE_MODE_LOOSE, 
                        result_buf, sizeof(result_buf), &result);

if (rc == LMJCORE_SUCCESS) {
    for (size_t i = 0; i < result->count; i++) {
        lmjcore_member_descriptor* desc = &result->descriptor.members[i];
        char* member_name = (char*)(result_buf + desc->name_offset);
        uint8_t* value = result_buf + desc->value_offset;
        
        printf("成员: %.*s, 值长度: %d\n", 
               desc->name_len, member_name, desc->value_len);
    }
}

lmjcore_txn_commit(txn);
lmjcore_cleanup(env);
```

## 📖 核心概念

### 指针系统
17字节固定长度指针，包含1字节类型前缀和16字节唯一标识：
```c

typedef uint8_t lmjcore_ptr[LMJCORE_PTR_LEN];

```

### 数据存储模型
- **对象 (Object)**：键值对集合，成员可动态增删
- **数组 (Array)**：有序值集合，支持追加操作

### 访问模式
- **宽松模式 (Loose Mode)**：自动补全缺失数据，保证查询完成
- **严格模式 (Strict Mode)**：遇到数据缺失立即终止，确保完整性

## 🔧 API 概览

### 环境管理
- `lmjcore_init()` - 初始化存储环境
- `lmjcore_cleanup()` - 清理资源

### 事务控制
- `lmjcore_txn_begin()` - 开始事务
- `lmjcore_txn_commit()` - 提交事务
- `lmjcore_txn_abort()` - 中止事务

### 对象操作
- `lmjcore_obj_create()` - 创建对象
- `lmjcore_obj_put()` - 写入对象成员
- `lmjcore_obj_get()` - 读取整个对象
- `lmjcore_obj_member_get()` - 读取特定成员

### 数组操作
- `lmjcore_arr_create()` - 创建数组
- `lmjcore_arr_append()` - 追加数组元素
- `lmjcore_arr_get()` - 读取数组内容

### 工具函数
- `lmjcore_ptr_to_string()` - 指针转可读字符串
- `lmjcore_ptr_from_string()` - 字符串转指针
- `lmjcore_exist()` - 检查指针有效性

## 🎯 性能特点

### 读取性能
- **对象获取**：O(log n) + k * O(1) 复杂度
- **直接寻址**：通过指针直接访问数据，无递归开销

### 写入优势
- **原子操作**：关键操作在单事务内完成
- **局部更新**：修改嵌套数据只需操作相关单元

## 🔍 适用场景

### ✅ 理想用例
- 高并发读取场景（配置中心、用户画像）
- 灵活 Schema 需求（快速迭代业务）
- 嵌入式环境（IoT设备、边缘计算）
- 作为底层存储引擎

### ❌ 不适用场景
- 复杂关联查询（需要上层索引）
- 大规模分析任务（OLAP场景）
- 强 Schema 约束数据

## 🛠️ 构建说明

```bash
git clone https://github.com/your-username/lmjcore.git
make
```

## 📊 基准测试(未完成)

```bash
# 运行性能测试
./benchmarks/read_benchmark
./benchmarks/write_benchmark

# 运行正确性测试
./tests/integration_test
```

## 🤝 贡献指南

我们欢迎各种形式的贡献！

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

## 📄 许可证

本项目采用 MIT 许可证 - 查看 [LICENSE](LICENSE) 文件了解详情。

## 🤖 关于本项目

**真诚透明**：作为新人开发者，这是我第一次在 GitHub 上传项目。在 LMJCore 的开发过程中，我大量使用了 AI 辅助进行代码实现、文档编写和架构设计。虽然核心设计理念是我的，但具体实现得到了 AI 的很大帮助。这是一个学习成长的过程，感谢理解！ 🙏

## 🙏 致谢

- 基于 [LMDB](https://symas.com/lmdb/) 构建，继承其卓越的存储特性
- 受现代数据库设计理念启发，特别是扁平化存储思想

## 📚 文档资源

- [核心设计定义](docs/core/LMJCore%20核心设计定义.md)
- [事务控制规范](docs/core/LMJCore%20事务.md)
- [实现细则指南](docs/core/LMJCore%20实现细则与集成指南.md)
- [架构愿景](docs/core/LMJCore%20愿景文档.md)

---

**LMJCore** - 为极致性能而生的嵌入式存储内核 🚀
