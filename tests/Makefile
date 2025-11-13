# Tests Makefile

# 配置
BUILD_DIR ?= ../build
CORE_DIR ?= ../core
CONFIG_TOOLKIT_DIR ?= ../Toolkit/config_obj_toolkit
RESULT_PARSER_DIR ?= ../Toolkit/result_parser
CFLAGS += -I$(CORE_DIR)/include -I$(CONFIG_TOOLKIT_DIR)/include -I$(RESULT_PARSER_DIR)/include

# 基础链接标志
BASE_LDFLAGS = -L$(BUILD_DIR) -Wl,-rpath,$(BUILD_DIR) -llmdb -luuid

# 测试程序
CONFIG_TEST_SRC = lmjcore_config_obj_test/config_obj_test.c
RESULT_PARSER_TEST_SRC = result_parser/result_parser_test.c
CORE_TEST_SRC = LMJCore_tests/LMJCoreTest.c
READ_TEST_SRC = LMJCore_tests/readTest.c
STRESS_TEST_SRC = LMJCore_tests/stressTest.c

TEST_TARGETS = \
	$(BUILD_DIR)/config_obj_test \
	$(BUILD_DIR)/result_parser_test \
	$(BUILD_DIR)/LMJCoreTest \
	$(BUILD_DIR)/readTest \
	$(BUILD_DIR)/stressTest

# 默认目标
.PHONY: all
all: $(TEST_TARGETS)

# 构建配置对象测试（依赖配置工具包和核心库）
$(BUILD_DIR)/config_obj_test: $(CONFIG_TEST_SRC) | $(BUILD_DIR)/liblmjconfig.so
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(BASE_LDFLAGS) -llmjcore -llmjconfig
	@echo "Built config_obj_test"

# 构建结果解析器测试（依赖结果解析器工具包和核心库）
$(BUILD_DIR)/result_parser_test: $(RESULT_PARSER_TEST_SRC) | $(BUILD_DIR)/liblmjresultparser.so
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(BASE_LDFLAGS) -llmjcore -llmjresultparser
	@echo "Built result_parser_test"

# 构建核心测试（只依赖核心库）
$(BUILD_DIR)/LMJCoreTest: $(CORE_TEST_SRC) | $(BUILD_DIR)/liblmjcore.so
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(BASE_LDFLAGS) -llmjcore
	@echo "Built LMJCoreTest"

# 构建读取测试（只依赖核心库）
$(BUILD_DIR)/readTest: $(READ_TEST_SRC) | $(BUILD_DIR)/liblmjcore.so
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(BASE_LDFLAGS) -llmjcore
	@echo "Built readTest"

# 构建压力测试（只依赖核心库）
$(BUILD_DIR)/stressTest: $(STRESS_TEST_SRC) | $(BUILD_DIR)/liblmjcore.so
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(BASE_LDFLAGS) -llmjcore
	@echo "Built stressTest"

# 确保依赖库存在
$(BUILD_DIR)/liblmjcore.so:
	$(MAKE) -C $(CORE_DIR)

$(BUILD_DIR)/liblmjconfig.so:
	$(MAKE) -C $(CONFIG_TOOLKIT_DIR)

$(BUILD_DIR)/liblmjresultparser.so:
	$(MAKE) -C $(RESULT_PARSER_DIR)

# 运行测试
.PHONY: test
test: all
	@echo "Running tests..."
	@for test in $(TEST_TARGETS); do \
		if [ -f $$test ]; then \
			echo "=== Running $$(basename $$test) ==="; \
			$$test || echo "Test $$(basename $$test) failed with exit code $$?"; \
			echo ""; \
		else \
			echo "$$test not found!"; \
		fi \
	done

# 清理
.PHONY: clean
clean:
	rm -f $(TEST_TARGETS)

# 显示信息
.PHONY: info
info:
	@echo "Test Programs:"
	@echo "  $(TEST_TARGETS)"
	@echo "Dependencies:"
	@echo "  Core: $(BUILD_DIR)/liblmjcore.so"
	@echo "  Config Toolkit: $(BUILD_DIR)/liblmjconfig.so"
	@echo "  Result Parser: $(BUILD_DIR)/liblmjresultparser.so"