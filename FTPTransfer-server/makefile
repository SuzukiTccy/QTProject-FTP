# 编译器
GCC = clang++

# 构建模式
CCMODE = PROGRAM

# 包含路径（macOS Homebrew安装的libevent）
INCLUDES = -I/opt/homebrew/opt/libevent/include

# 编译标志
CFLAGS = -Wall -std=c++17 -O2

# 目标文件名
TARGET = ftpSrv

# 源文件（所有.cpp文件）
SRCS := $(wildcard *.cpp)

# 链接库
LIBS = -L/opt/homebrew/opt/libevent/lib -levent -lpthread

# 主构建规则（可执行程序）
ifeq ($(CCMODE), PROGRAM)
$(TARGET): $(SRCS)
	$(GCC) $(CFLAGS) $(INCLUDES) -o $(TARGET) $(SRCS) $(LIBS)
	@chmod +x $(TARGET)
	@echo "构建 $(TARGET) 成功！"
endif

# 清理规则
clean:
	rm -f $(TARGET)
	@echo "已清理 $(TARGET)"

# 安装规则（如果需要）
install: $(TARGET)
	@echo "install 命令尚未实现"

# 伪目标声明
.PHONY: all clean install