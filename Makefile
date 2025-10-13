# 编译器以及标志
CC = gcc
CFLAGS = -I./include
LIBS = -llmdb -luuid

# 目录设置
LMJCORE_SRC_DIR = src/core
TESTS_DIR = tests
BUILD = build

# 源文件和目标文件
LMJCORE_SRCS = $(wildcard $(LMJCORE_SRC_DIR)/*.c)
LMJCORE_OBJS = $(patsubst $(LMJCORE_SRC_DIR)/%.c,$(BUILD)/%.o,$(LMJCORE_SRCS))

TEST_TARGET = bin/LMJCoreTest

# 默认目标
all: $(TEST_TARGET)

# 创建构建目录
$(BUILD):
	mkdir -p $(BUILD)

# 编译 LMJCore 源文件
$(BUILD)/%.o: $(LMJCORE_SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# 链接测试程序
$(TEST_TARGET): $(LMJCORE_OBJS) $(TESTS_DIR)/LMJCore_tests/LMJCoreTest.c
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(TESTS_DIR)/LMJCore_tests/LMJCoreTest.c $(LMJCORE_OBJS) $(LIBS)

# 如果需要 lmdb，添加相应的规则
# lmdb:
#     $(MAKE) -C thirdparty/lmdb

# 清理
clean:
	rm -rf $(BUILD) bin

.PHONY: all clean