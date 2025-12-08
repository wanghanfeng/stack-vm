#ifndef STACK_VM_H
#define STACK_VM_H

#include <stdint.h>
#include <stdbool.h>

// --------------- 类型系统 ---------------

// 常量定义
#define MAX_PROPS 64

// 值类型枚举
typedef enum {
    VAL_NUMBER,
    VAL_STRING,
    VAL_BOOLEAN,
    VAL_UNDEFINED,
    VAL_NULL,
    VAL_OBJECT
} ValueType;

// 前向声明
typedef struct Value Value;

// 对象头
typedef struct ObjectHeader {
    int ref_count;
    ValueType type;
} ObjectHeader;

// 字符串对象
typedef struct {
    ObjectHeader header;
    size_t length;
    char* chars;
} StringObject;

// 基础对象
typedef struct {
    ObjectHeader header;
    int property_count;
    char** prop_names;
    Value* prop_values;
} Object;

// 值类型
typedef struct Value {
    ValueType type;
    union {
        double number;
        bool boolean;
        ObjectHeader* obj;
    } data;
} Value;

// --------------- 变量环境 ---------------
#define MAX_VARS 32

typedef struct Env Env;

// 环境结构体
typedef struct Env {
    char* names[MAX_VARS];
    Value values[MAX_VARS];
    int var_count;
    Env* parent;
} Env;

// --------------- 栈式虚拟机 ---------------
typedef struct {
    Value stack[64];
    int sp;
    Env* current_env;
    int call_stack[16];
    int call_sp;
} StackVM;

// --------------- 字节码指令 ---------------
typedef enum {
    OP_PUSH_NUM,
    OP_PUSH_STR,
    OP_PUSH_BOOL,
    OP_PUSH_UNDEFINED,
    OP_PUSH_NULL,
    OP_PUSH_VAR,
    OP_STORE_VAR,
    OP_ADD,
    OP_CALL,
    OP_RET,
    OP_PRINT,
    OP_EXIT,
    OP_NEW_OBJECT,
    OP_SET_PROP,
    OP_GET_PROP,
    OP_PUSH_ENV,
    OP_POP_ENV
} OpCode;

// --------------- 函数声明 ---------------

// 值操作
Value val_number(double num);
Value val_boolean(bool b);
Value val_undefined();
Value val_null();
Value val_string(const char* str);
Value val_object();
void val_free(Value v);

// 环境操作
Value env_get(Env* env, const char* name);
void env_set(Env* env, const char* name, Value val);
Env* create_env(Env* parent);
void free_env(Env* env);

// 虚拟机操作
void vm_init(StackVM* vm);
void vm_push(StackVM* vm, Value val);
Value vm_pop(StackVM* vm);
void vm_pop_free(StackVM* vm);
void vm_call(StackVM* vm, int func_ip);
int vm_ret(StackVM* vm);
void vm_execute(StackVM* vm, const uint8_t* bytecode, int len);

#endif // STACK_VM_H