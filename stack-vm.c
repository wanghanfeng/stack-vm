#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// --------------- 进阶特性：类型系统（适配 JS 动态类型）---------------
typedef enum {
    VAL_NUMBER,  // 数值类型
    VAL_STRING,  // 字符串类型
    VAL_NIL      // 空值（类似 JS undefined）
} ValueType;

typedef struct {
    ValueType type;
    union {
        double number;
        char* string;
    } data;
} Value;

// --------------- 进阶特性：变量环境（类似 JS 作用域链，简化为单环境）---------------
#define MAX_VARS 32
typedef struct {
    char* names[MAX_VARS];  // 变量名
    Value values[MAX_VARS]; // 变量值
    int var_count;          // 已定义变量数
} Env;

// --------------- 栈式虚拟机（新增环境指针、函数调用栈）---------------
typedef struct {
    Value stack[64];  // 操作数栈（存储 Value 类型，支持多类型）
    int sp;           // 栈指针
    Env env;          // 变量环境
    int call_stack[16];// 函数调用栈（存储指令指针 ip，支持嵌套调用）
    int call_sp;      // 调用栈指针
} StackVM;

// --------------- 字节码指令（新增变量、函数相关指令）---------------
typedef enum {
    OP_PUSH_NUM,    // 压入数值
    OP_PUSH_STR,    // 压入字符串（后续跟字符串长度+字节流）
    OP_PUSH_VAR,    // 压入变量（后续跟变量名长度+字节流）
    OP_STORE_VAR,   // 存储变量（栈顶值 → 变量）
    OP_ADD,         // 加法（支持数值+数值、字符串+字符串）
    OP_CALL,        // 函数调用（后续跟函数起始指令偏移）
    OP_RET,         // 函数返回
    OP_PRINT,       // 打印栈顶值
    OP_EXIT         // 退出
} OpCode;

// --------------- 工具函数（类型创建/销毁、变量查找）---------------
Value val_number(double num) {
    Value v = {.type = VAL_NUMBER};
    v.data.number = num;
    return v;
}

Value val_string(const char* str) {
    Value v = {.type = VAL_STRING};
    v.data.string = strdup(str); // 复制字符串，避免野指针
    return v;
}

Value val_nil() {
    Value v = {.type = VAL_NIL};
    return v;
}

void val_free(Value v) {
    if (v.type == VAL_STRING) free(v.data.string);
}

// 查找变量（不存在返回空值）
Value env_get(Env* env, const char* name) {
    for (int i = 0; i < env->var_count; i++) {
        if (strcmp(env->names[i], name) == 0) {
            Value val = env->values[i];
            // 如果是字符串类型，需要复制一份，避免双重释放
            if (val.type == VAL_STRING) {
                val.data.string = strdup(val.data.string);
            }
            return val;
        }
    }
    return val_nil();
}

// 存储变量（已存在则覆盖，不存在则新增）
void env_set(Env* env, const char* name, Value val) {
    for (int i = 0; i < env->var_count; i++) {
        if (strcmp(env->names[i], name) == 0) {
            val_free(env->values[i]); // 释放旧值
            env->values[i] = val;
            return;
        }
    }
    if (env->var_count >= MAX_VARS) {
        fprintf(stderr, "变量数量超限！\n");
        exit(1);
    }
    env->names[env->var_count] = strdup(name);
    env->values[env->var_count] = val;
    env->var_count++;
}

// --------------- 虚拟机核心操作 ---------------
void vm_init(StackVM* vm) {
    vm->sp = 0;
    vm->call_sp = 0;
    vm->env.var_count = 0;
}

void vm_push(StackVM* vm, Value val) {
    if (vm->sp >= 64) {fprintf(stderr, "栈溢出！\n"); exit(1);}
    vm->stack[vm->sp++] = val;
}

Value vm_pop(StackVM* vm) {
    if (vm->sp <= 0) {fprintf(stderr, "栈下溢！\n"); exit(1);}
    return vm->stack[--vm->sp];
}

// 函数调用：保存当前 ip 到调用栈，跳转到函数起始位置
void vm_call(StackVM* vm, int func_ip) {
    if (vm->call_sp >= 16) {fprintf(stderr, "调用栈溢出！\n"); exit(1);}
    vm->call_stack[vm->call_sp++] = func_ip;
}

// 函数返回：从调用栈恢复 ip
int vm_ret(StackVM* vm) {
    if (vm->call_sp <= 0) {fprintf(stderr, "无函数可返回！\n"); exit(1);}
    return vm->call_stack[--vm->call_sp];
}

// --------------- 解释器（支持多类型运算、变量、函数）---------------
void vm_execute(StackVM* vm, const uint8_t* bytecode, int len) {
    int ip = 0;
    while (ip < len) {
        OpCode op = (OpCode)bytecode[ip++];
        switch (op) {
            // 压入数值：后续 8 字节为 double
            case OP_PUSH_NUM: {
                double num;
                memcpy(&num, &bytecode[ip], sizeof(double));
                vm_push(vm, val_number(num));
                ip += sizeof(double);
                break;
            }
            // 压入字符串：1字节长度 + N字节字符串
            case OP_PUSH_STR: {
                uint8_t str_len = bytecode[ip++];
                char str[str_len + 1];
                memcpy(str, &bytecode[ip], str_len);
                str[str_len] = '\0';
                vm_push(vm, val_string(str));
                ip += str_len;
                break;
            }
            // 压入变量：1字节长度 + N字节变量名
            case OP_PUSH_VAR: {
                uint8_t name_len = bytecode[ip++];
                char name[name_len + 1];
                memcpy(name, &bytecode[ip], name_len);
                name[name_len] = '\0';
                Value val = env_get(&vm->env, name);
                if (val.type == VAL_NIL) {
                    fprintf(stderr, "未定义变量：%s\n", name);
                    exit(1);
                }
                vm_push(vm, val);
                ip += name_len;
                break;
            }
            // 存储变量：栈顶值 → 变量（变量名在栈顶值之后）
            case OP_STORE_VAR: {
                uint8_t name_len = bytecode[ip++];
                char name[name_len + 1];
                memcpy(name, &bytecode[ip], name_len);
                name[name_len] = '\0';
                Value val = vm_pop(vm);
                env_set(&vm->env, name, val);
                ip += name_len;
                break;
            }
            // 加法：支持数值+数值、字符串+字符串、字符串+数值（类似 JS 隐式转换）
            case OP_ADD: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (a.type == VAL_NUMBER && b.type == VAL_NUMBER) {
                    vm_push(vm, val_number(a.data.number + b.data.number));
                } else if (a.type == VAL_STRING && b.type == VAL_STRING) {
                    char* str = malloc(strlen(a.data.string) + strlen(b.data.string) + 1);
                    strcpy(str, a.data.string);
                    strcat(str, b.data.string);
                    Value res = {.type = VAL_STRING, .data.string = str};
                    vm_push(vm, res);
                } else if (a.type == VAL_STRING && b.type == VAL_NUMBER) {
                    // 字符串 + 数值：将数值转换为字符串后拼接
                    char num_str[32];
                    sprintf(num_str, "%.2f", b.data.number);
                    char* str = malloc(strlen(a.data.string) + strlen(num_str) + 1);
                    strcpy(str, a.data.string);
                    strcat(str, num_str);
                    Value res = {.type = VAL_STRING, .data.string = str};
                    vm_push(vm, res);
                } else {
                    fprintf(stderr, "不支持的加法类型！\n");
                    exit(1);
                }
                val_free(a);
                val_free(b);
                break;
            }
            // 函数调用：后续 4 字节为函数起始指令偏移（小端）
            case OP_CALL: {
                int func_offset;
                memcpy(&func_offset, &bytecode[ip], sizeof(int));
                ip += sizeof(int);
                vm_call(vm, ip); // 保存当前 ip（函数执行完后返回此处）
                ip = func_offset; // 跳转到函数起始位置
                break;
            }
            // 函数返回：恢复 ip 到调用前位置
            case OP_RET: {
                ip = vm_ret(vm);
                break;
            }
            // 打印：支持多类型输出
            case OP_PRINT: {
                Value val = vm_pop(vm);
                switch (val.type) {
                    case VAL_NUMBER: printf("输出：%.2f\n", val.data.number); break;
                    case VAL_STRING: printf("输出：%s\n", val.data.string); break;
                    case VAL_NIL: printf("输出：nil\n"); break;
                }
                val_free(val);
                break;
            }
            case OP_EXIT:
                return;
            default:
                fprintf(stderr, "未知指令：%d\n", op);
                exit(1);
        }
    }
}

// 测试：执行「变量赋值 + 函数调用 + 字符串拼接 + 数值运算」
int main() {
    StackVM vm;
    vm_init(&vm);

    // 字节码序列：函数调用测试
    // 1. 定义变量
    // 2. 调用函数 helloFunction
    // 3. 打印函数返回结果
    uint8_t bytecode[] = {
        // 1. 定义变量 name
        OP_PUSH_STR, 5, 'A','l','i','c','e',  // 压入字符串 "Alice"（长度5）
        OP_STORE_VAR, 4, 'n','a','m','e',     // 存储到变量 name（长度4）
        
        // 2. 调用函数 helloFunction
        OP_CALL, 20, 0x00, 0x00, 0x00,          // 调用函数，函数起始偏移量为20
        
        OP_PRINT,                               // 打印函数返回结果
        OP_EXIT,                                // 退出
        
        // 3. 函数定义：helloFunction() -> "Hello " + name
        // 函数从偏移量20开始
        OP_PUSH_STR, 6, 'H','e','l','l','o',' ',  // 压入 "Hello "（长度6）
        OP_PUSH_VAR, 4, 'n','a','m','e',        // 压入变量 name
        OP_ADD,                                 // 拼接："Hello " + name
        OP_RET                                  // 函数返回
    };

    vm_execute(&vm, bytecode, sizeof(bytecode));

    // 释放变量环境内存
    for (int i = 0; i < vm.env.var_count; i++) {
        free(vm.env.names[i]);
        val_free(vm.env.values[i]);
    }
    return 0;
}