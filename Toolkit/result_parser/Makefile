# Result Parser Makefile

# 配置（从上层继承）
BUILD_DIR ?= ../../build
CORE_DIR ?= ../../core
CFLAGS += -fPIC -I$(CORE_DIR)/include
LDFLAGS += -L$(BUILD_DIR) -llmjcore

# 项目特定配置
LIB_NAME = liblmjresultparser
LIB_SO = $(LIB_NAME).so

# 源文件和头文件
SRC_DIR = src
INCLUDE_DIR = include
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/toolkit/%.o)
HEADERS = $(wildcard $(INCLUDE_DIR)/*.h)

# 默认目标
.PHONY: all
all: $(BUILD_DIR)/$(LIB_SO)

# 创建共享库
$(BUILD_DIR)/$(LIB_SO): $(OBJECTS) | $(BUILD_DIR)/liblmjcore.so
	@mkdir -p $(BUILD_DIR)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
	@echo "Built result parser: $(LIB_SO)"

# 编译对象文件
$(BUILD_DIR)/toolkit/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# 确保核心库存在
$(BUILD_DIR)/liblmjcore.so:
	$(MAKE) -C $(CORE_DIR)

# 安装头文件到构建目录
.PHONY: install-headers
install-headers: $(BUILD_DIR)/include/result_parser.h

$(BUILD_DIR)/include/result_parser.h: $(INCLUDE_DIR)/result_parser.h
	@mkdir -p $(BUILD_DIR)/include
	cp $< $@

# 清理
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)/toolkit/result_parser.o
	rm -f $(BUILD_DIR)/$(LIB_SO)

# 显示信息
.PHONY: info
info:
	@echo "Result Parser Info:"
	@echo "  Sources: $(SOURCES)"
	@echo "  Headers: $(HEADERS)"
	@echo "  Dependencies: liblmjcore"