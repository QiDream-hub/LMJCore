# Core Library Makefile

# 配置（从上层继承）
BUILD_DIR ?= ../build
CFLAGS += -fPIC

# 项目特定配置
LIB_NAME = liblmjcore
LIB_SO = $(LIB_NAME).so

# 源文件和头文件
SRC_DIR = src
INCLUDE_DIR = include
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/core/%.o)
HEADERS = $(wildcard $(INCLUDE_DIR)/*.h)

# 默认目标
.PHONY: all
all: $(BUILD_DIR)/$(LIB_SO)

# 创建共享库
$(BUILD_DIR)/$(LIB_SO): $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
	@echo "Built core library: $(LIB_SO)"

# 编译对象文件
$(BUILD_DIR)/core/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# 安装头文件到构建目录
.PHONY: install-headers
install-headers: $(BUILD_DIR)/include/lmjcore.h

$(BUILD_DIR)/include/lmjcore.h: $(INCLUDE_DIR)/lmjcore.h
	@mkdir -p $(BUILD_DIR)/include
	cp $< $@

# 清理
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)/core
	rm -f $(BUILD_DIR)/$(LIB_SO)
	rm -f $(BUILD_DIR)/include/lmjcore.h

# 显示信息
.PHONY: info
info:
	@echo "Core Library Info:"
	@echo "  Sources: $(SOURCES)"
	@echo "  Headers: $(HEADERS)"
	@echo "  Output: $(LIB_SO)"