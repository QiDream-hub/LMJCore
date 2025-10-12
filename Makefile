# 编译器以及标志
CC = gcc
CFLAGS = -I./include
LIBS = -llmdb -luuid

# 目录设置
LMJCORE_SRC_DIR = src
TESTS_DIR = tests

# 目标文件
LMJCORE_OBJS = $(patsubst $(LMJCORE_SRC_DIR)/%.c,%.o,$(wildcard $(LMJCORE_SRC_DIR)/*.c))
TEST_TARGET = bin/LMJCoreTest

# 默认目标 - 添加 lmdb 依赖
all: lmdb $(TEST_TARGET)

# 编译 LMJCore 源文件
%.o: $(LMJCORE_SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 链接测试程序
$(TEST_TARGET): $(LMJCORE_OBJS) $(TESTS_DIR)/LMJCore_tests/LMJCoreTest.c
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(TESTS_DIR)/LMJCore_tests/LMJCoreTest.c $(LMJCORE_OBJS) $(LDFLAGS) $(LIBS)

# 清理
clean:
	rm -f *.o $(TEST_TARGET)
# 	$(MAKE) -C thirdparty/lmdb clean

.PHONY: all clean