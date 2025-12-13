# 顶层 Makefile - LMJCore 项目构建系统

# 配置
export BUILD_DIR = $(CURDIR)/build
export CC = gcc
export CFLAGS = -Wall -Wextra -g -I$(CURDIR)/core/include
export LDFLAGS = -llmdb
export LD_LIBRARY_PATH = $(BUILD_DIR)
export TEST_BIN = $(BUILD_DIR)/bin

# 默认目标
.PHONY: all
all: core toolkit tests

# 构建核心库（依赖外部库 lmdb, uuid）
.PHONY: core
core:
	@echo "Building LMJCore library..."
	$(MAKE) -C core

# 构建配置工具包（依赖核心库）
.PHONY: toolkit
toolkit: core
	@echo "Building config toolkit..."
	$(MAKE) -C Toolkit/config_obj_toolkit
	@echo "Building result parser..."
	$(MAKE) -C Toolkit/result_parser
	@echo "Building ptr gender..."
	$(MAKE) -C Toolkit/ptr_uuid_gen

# 构建测试程序（依赖核心库和工具包）
.PHONY: tests
tests: core toolkit
	@echo "Building tests..."
	$(MAKE) -C tests

# 清理所有构建产物
.PHONY: clean
clean:
	$(MAKE) -C core clean
	$(MAKE) -C Toolkit/config_obj_toolkit clean
	$(MAKE) -C Toolkit/ptr_uuid_gen clean
	$(MAKE) -C Toolkit/result_parser clean
	$(MAKE) -C tests clean
	rm -rf $(BUILD_DIR)


# 检查依赖
.PHONY: check-deps
check-deps:
	@echo "Checking dependencies..."
	@pkg-config --exists lmdb && echo "✓ lmdb found" || echo "✗ lmdb not found"
	@echo "Dependency check completed"

# 运行测试（设置库路径）
.PHONY: test
test: all
	@echo "Running tests with LD_LIBRARY_PATH=$(BUILD_DIR)..."
	LD_LIBRARY_PATH=$(BUILD_DIR) $(MAKE) -C tests test

# 显示项目信息
.PHONY: info
info:
	@echo "LMJCore Build System"
	@echo "Build Directory: $(BUILD_DIR)"
	@echo "Dependencies: lmdb, uuid"
	@echo "Library Path: $(LD_LIBRARY_PATH)"