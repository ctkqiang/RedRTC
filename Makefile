# REd RTC Signaling Server Makefile
# High-performance WebRTC signaling server

# Project information
PROJECT_NAME = redrtc
VERSION = 1.0.0
BUILD_DATE = $(shell date +%Y-%m-%d_%H:%M:%S)

# Compiler and flags
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic \
         -Werror=return-type -Werror=implicit-function-declaration \
         -D_FORTIFY_SOURCE=2 -fstack-protector-strong \
         -D_GNU_SOURCE -D_DEFAULT_SOURCE \
         -DVERSION=\"$(VERSION)\" -DBUILD_DATE=\"$(BUILD_DATE)\"

# Debug flags
DEBUG_CFLAGS = -g -DDEBUG -O0

# Release flags  
RELEASE_CFLAGS = -O2 -DNDEBUG

# Directories
SRCDIR = src
INCDIR = include
BUILDDIR = build
BINDIR = $(BUILDDIR)/bin
OBJDIR = $(BUILDDIR)/obj

# Source files
SRC_FILES = $(SRCDIR)/client.c $(SRCDIR)/message.c $(SRCDIR)/room.c $(SRCDIR)/server.c $(SRCDIR)/utilities.c
MAIN_SOURCE = redrtc.c

# Object files
OBJECTS = $(SRC_FILES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
MAIN_OBJECT = $(OBJDIR)/redrtc.o

# Binary target
TARGET = $(BINDIR)/$(PROJECT_NAME)

# Library paths (macOS Homebrew specific)
BREW_PREFIX = $(shell brew --prefix 2>/dev/null || echo "/usr/local")
OPENSSL_CFLAGS = -I$(BREW_PREFIX)/opt/openssl/include
OPENSSL_LIBS = -L$(BREW_PREFIX)/opt/openssl/lib -lssl -lcrypto

# Combined flags
CFLAGS += -I$(INCDIR) $(OPENSSL_CFLAGS)
LIBS = -lwebsockets -ljansson $(OPENSSL_LIBS) -lm -pthread

# Colors for output
RED = \033[0;31m
GREEN = \033[0;32m
YELLOW = \033[0;33m
BLUE = \033[0;34m
CYAN = \033[0;36m
NC = \033[0m

# Default target
all: release

# Release build
release: CFLAGS += $(RELEASE_CFLAGS)
release: $(TARGET)
	@echo "$(GREEN)✓ 编译完成: $(TARGET)$(NC)"
	@echo "$(CYAN)版本: $(VERSION) | 编译时间: $(BUILD_DATE)$(NC)"

# Debug build
debug: CFLAGS += $(DEBUG_CFLAGS)
debug: $(TARGET)
	@echo "$(GREEN)✓ 调试版本编译完成: $(TARGET)$(NC)"
	@echo "$(CYAN)版本: $(VERSION) | 编译时间: $(BUILD_DATE)$(NC)"

# Main target
$(TARGET): $(MAIN_OBJECT) $(OBJECTS)
	@mkdir -p $(BINDIR)
	@echo "$(BLUE)链接目标文件...$(NC)"
	$(CC) $(MAIN_OBJECT) $(OBJECTS) $(LIBS) -o $(TARGET)
	@echo "$(GREEN)✓ 可执行文件创建成功: $(TARGET)$(NC)"

# Compile main source
$(OBJDIR)/redrtc.o: redrtc.c
	@mkdir -p $(OBJDIR)
	@echo "$(BLUE)编译主文件: $<$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	@echo "$(BLUE)编译: $<$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

# Check dependencies
check-deps:
	@echo "$(BLUE)检查依赖...$(NC)"
	@if [ -f "$(BREW_PREFIX)/opt/openssl/include/openssl/ssl.h" ]; then \
		echo "$(GREEN)✓ OpenSSL 找到$(NC)"; \
	else \
		echo "$(RED)✗ 错误: 找不到 OpenSSL$(NC)"; \
		echo "$(YELLOW)请安装: brew install openssl$(NC)"; \
		exit 1; \
	fi
	@if [ -f "$(BREW_PREFIX)/include/libwebsockets.h" ] || [ -f "/usr/local/include/libwebsockets.h" ]; then \
		echo "$(GREEN)✓ libwebsockets 找到$(NC)"; \
	else \
		echo "$(RED)✗ 错误: 找不到 libwebsockets$(NC)"; \
		echo "$(YELLOW)请安装: brew install libwebsockets$(NC)"; \
		exit 1; \
	fi
	@if [ -f "$(BREW_PREFIX)/include/jansson.h" ] || [ -f "/usr/local/include/jansson.h" ]; then \
		echo "$(GREEN)✓ jansson 找到$(NC)"; \
	else \
		echo "$(RED)✗ 错误: 找不到 jansson$(NC)"; \
		echo "$(YELLOW)请安装: brew install jansson$(NC)"; \
		exit 1; \
	fi

# Clean build artifacts
clean:
	@echo "$(YELLOW)清理构建文件...$(NC)"
	rm -rf $(BUILDDIR)
	@echo "$(GREEN)✓ 清理完成$(NC)"

# Run the server in foreground
run: debug
	@echo "$(BLUE)启动 $(PROJECT_NAME)...$(NC)"
	./$(TARGET) --verbose

# Show help
help:
	@echo "$(CYAN)REd RTC 信令服务器 - Makefile 使用说明$(NC)"
	@echo ""
	@echo "$(BLUE)可用目标:$(NC)"
	@echo "  $(GREEN)all$(NC)        - 编译发布版本 (默认)"
	@echo "  $(GREEN)debug$(NC)      - 编译调试版本"
	@echo "  $(GREEN)clean$(NC)      - 清理构建文件"
	@echo "  $(GREEN)run$(NC)        - 编译并运行 (调试模式)"
	@echo "  $(GREEN)check-deps$(NC) - 检查依赖"
	@echo "  $(GREEN)help$(NC)       - 显示此帮助信息"

.PHONY: all release debug clean run check-deps help