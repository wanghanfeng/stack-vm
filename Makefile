# Stack VM 编译器 Makefile

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99

# 显示帮助信息
help:
	@echo "Stack VM 编译器 Makefile 帮助"
	@echo "================================"
	@echo "使用方法: make [目标]"
	@echo ""
	@echo "可用目标:"
	@echo "  all         - 编译 stack-vm-compiler (默认目标)"
	@echo "  stack-vm-compiler - 编译编译器"
	@echo "  stack-vm    - 编译虚拟机"
	@echo "  clean       - 清理编译产物"
	@echo "  test        - 运行测试"
	@echo "  help        - 显示此帮助信息"
	@echo ""
	@echo "示例:"
	@echo "  make         # 编译编译器"
	@echo "  make clean   # 清理所有编译产物"
	@echo "  make test    # 运行测试"

# 目标文件
all: stack-vm-compiler

# 编译编译器
stack-vm-compiler: stack-vm-compiler.c stack-vm.c stack-vm.h
	$(CC) $(CFLAGS) -DCOMPILER_TEST -o stack-vm-compiler stack-vm-compiler.c stack-vm.c

# 编译虚拟机（用于直接测试虚拟机）
stack-vm: stack-vm.c stack-vm.h
	$(CC) $(CFLAGS) -o stack-vm stack-vm.c

# 清理编译产物
clean:
	rm -f stack-vm-compiler stack-vm *.o *.bin

# 测试编译器
test:
	@echo "创建测试文件..."
	@echo 'var x = 10; var y = 20; var z = x + y; print(z);' > test.txt
	@echo "测试直接执行..."
	./stack-vm-compiler -e test.txt
	@echo "测试编译到文件..."
	./stack-vm-compiler test.txt test.bin
	@echo "测试输出到标准输出..."
	./stack-vm-compiler -c test.txt | hexdump -C | head -20
	@echo "清理测试文件..."
	@rm -f test.txt test.bin
	@echo "所有测试通过！"

.PHONY: all clean test