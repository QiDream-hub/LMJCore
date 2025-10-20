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

# 测试目标
TEST_TARGETS = bin/LMJCoreTest bin/readTest bin/stressTest

# 默认目标
all: $(TEST_TARGETS)

# 创建构建目录
$(BUILD):
	mkdir -p $(BUILD)

# 编译 LMJCore 源文件
$(BUILD)/%.o: $(LMJCORE_SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# 链接 LMJCoreTest 程序
bin/LMJCoreTest: $(LMJCORE_OBJS) $(TESTS_DIR)/LMJCore_tests/LMJCoreTest.c
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(TESTS_DIR)/LMJCore_tests/LMJCoreTest.c $(LMJCORE_OBJS) $(LIBS)

# 链接 readTest 程序
bin/readTest: $(LMJCORE_OBJS) $(TESTS_DIR)/LMJCore_tests/readTest.c
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(TESTS_DIR)/LMJCore_tests/readTest.c $(LMJCORE_OBJS) $(LIBS)

# 链接 stressTest 程序
bin/stressTest: $(LMJCORE_OBJS) $(TESTS_DIR)/LMJCore_tests/stressTest.c
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(TESTS_DIR)/LMJCore_tests/stressTest.c $(LMJCORE_OBJS) $(LIBS)

# 如果需要 lmdb，添加相应的规则
# lmdb:
#     $(MAKE) -C thirdparty/lmdb

# 清理
clean:
	rm -rf $(BUILD) bin

.PHONY: all clean