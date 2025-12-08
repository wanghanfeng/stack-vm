#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

// --------------- 进阶特性：类型系统（适配 JS 动态类型）---------------

// 常量定义
#define MAX_PROPS 64 // 每个对象的最大属性数

// 值类型枚举
typedef enum {
    VAL_NUMBER,  // 数值类型
    VAL_STRING,  // 字符串类型
    VAL_BOOLEAN, // 布尔类型
    VAL_UNDEFINED, // undefined
    VAL_NULL,    // null
    VAL_OBJECT   // 对象类型（基础对象支持）
} ValueType;

// 前向声明值类型
typedef struct Value Value;

// 对象头，用于内存管理和类型标记
typedef struct ObjectHeader {
    int ref_count;   // 引用计数
    ValueType type;  // 对象类型
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
        double number;     // 数值
        bool boolean;      // 布尔值
        ObjectHeader* obj; // 对象（字符串、普通对象等）
    } data;
} Value;

// 函数原型声明
void val_free(Value v);

// --------------- 进阶特性：变量环境（类似 JS 作用域链）---------------
#define MAX_VARS 32
typedef struct Env Env;

// 环境结构体，支持作用域链
typedef struct Env {
    char* names[MAX_VARS];  // 变量名
    Value values[MAX_VARS]; // 变量值
    int var_count;          // 已定义变量数
    Env* parent;            // 父环境指针，形成作用域链
} Env;

// --------------- 栈式虚拟机（新增环境指针、函数调用栈）---------------
typedef struct {
    Value stack[64];  // 操作数栈（存储 Value 类型，支持多类型）
    int sp;           // 栈指针
    Env* current_env; // 当前环境指针（指向作用域链的顶部）
    int call_stack[16];// 函数调用栈（存储指令指针 ip，支持嵌套调用）
    int call_sp;      // 调用栈指针
} StackVM;

// --------------- 字节码指令（新增变量、函数相关指令）---------------
typedef enum {
    OP_PUSH_NUM,    // 压入数值
    OP_PUSH_STR,    // 压入字符串（后续跟字符串长度+字节流）
    OP_PUSH_BOOL,   // 压入布尔值（后续1字节）
    OP_PUSH_UNDEFINED, // 压入undefined
    OP_PUSH_NULL,   // 压入null
    OP_PUSH_VAR,    // 压入变量（后续跟变量名长度+字节流）
    OP_STORE_VAR,   // 存储变量（栈顶值 → 变量）
    OP_ADD,         // 加法（支持多种类型）
    OP_CALL,        // 函数调用（后续跟函数起始指令偏移）
    OP_RET,         // 函数返回
    OP_PRINT,       // 打印栈顶值
    OP_EXIT,        // 退出
    OP_NEW_OBJECT,  // 创建新对象
    OP_SET_PROP,    // 设置对象属性（栈顶：值，栈次顶：对象，后续：属性名）
    OP_GET_PROP,    // 获取对象属性（栈顶：对象，后续：属性名）
    OP_PUSH_ENV,    // 创建新作用域（压入当前环境，创建新环境作为当前环境）
    OP_POP_ENV      // 退出当前作用域（恢复之前的环境）
} OpCode;

// --------------- 内存管理工具函数 ---------------
// 创建对象头
ObjectHeader* create_object(ValueType type, size_t size) {
    ObjectHeader* obj = malloc(size);
    if (!obj) {
        fprintf(stderr, "内存分配失败！\n");
        exit(1);
    }
    obj->ref_count = 1;
    obj->type = type;
    return obj;
}

// 增加引用计数
void gc_inc_ref(ObjectHeader* obj) {
    if (obj) obj->ref_count++;
}

// 减少引用计数，如果引用为0则释放
void gc_dec_ref(ObjectHeader* obj) {
    if (!obj) return;
    if (--obj->ref_count == 0) {
        switch (obj->type) {
            case VAL_STRING: {
                StringObject* str_obj = (StringObject*)obj;
                free(str_obj->chars);
                break;
            }
            case VAL_OBJECT: {
                Object* obj_obj = (Object*)obj;
                for (int i = 0; i < obj_obj->property_count; i++) {
                    free(obj_obj->prop_names[i]);
                    val_free(obj_obj->prop_values[i]);
                }
                free(obj_obj->prop_names);
                free(obj_obj->prop_values);
                break;
            }
            default:
                break;
        }
        free(obj);
    }
}

// --------------- 工具函数（类型创建/销毁、变量查找）---------------
Value val_number(double num) {
    Value v = {.type = VAL_NUMBER};
    v.data.number = num;
    return v;
}

Value val_boolean(bool b) {
    Value v = {.type = VAL_BOOLEAN};
    v.data.boolean = b;
    return v;
}

Value val_undefined() {
    Value v = {.type = VAL_UNDEFINED};
    return v;
}

Value val_null() {
    Value v = {.type = VAL_NULL};
    return v;
}

Value val_string(const char* str) {
    Value v = {.type = VAL_STRING};
    size_t len = strlen(str);
    StringObject* str_obj = (StringObject*)create_object(VAL_STRING, sizeof(StringObject));
    str_obj->length = len;
    str_obj->chars = malloc(len + 1);
    strcpy(str_obj->chars, str);
    v.data.obj = (ObjectHeader*)str_obj;
    return v;
}

// 创建一个新对象
Value val_object() {
    Value v = {.type = VAL_OBJECT};
    Object* obj = (Object*)create_object(VAL_OBJECT, sizeof(Object));
    obj->property_count = 0;
    obj->prop_names = NULL;
    obj->prop_values = NULL;
    v.data.obj = (ObjectHeader*)obj;
    return v;
}

// 释放值
void val_free(Value v) {
    switch (v.type) {
        case VAL_STRING:
        case VAL_OBJECT:
            gc_dec_ref(v.data.obj);
            break;
        default:
            break;
    }
}

// 查找变量（沿作用域链查找，不存在返回undefined）
Value env_get(Env* env, const char* name) {
    Env* current = env;
    while (current != NULL) {
        for (int i = 0; i < current->var_count; i++) {
            if (strcmp(current->names[i], name) == 0) {
                Value val = current->values[i];
                // 增加引用计数，因为返回的是新的引用
                if (val.type == VAL_STRING || val.type == VAL_OBJECT) {
                    gc_inc_ref(val.data.obj);
                }
                return val;
            }
        }
        current = current->parent; // 继续查找父环境
    }
    return val_undefined();
}

// 存储变量（只在当前环境中设置，已存在则覆盖，不存在则新增）
void env_set(Env* env, const char* name, Value val) {
    for (int i = 0; i < env->var_count; i++) {
        if (strcmp(env->names[i], name) == 0) {
            val_free(env->values[i]); // 释放旧值
            // 增加新值的引用计数，因为它被环境持有
            if (val.type == VAL_STRING || val.type == VAL_OBJECT) {
                gc_inc_ref(val.data.obj);
            }
            env->values[i] = val;
            return;
        }
    }
    if (env->var_count >= MAX_VARS) {
        fprintf(stderr, "变量数量超限！\n");
        exit(1);
    }
    env->names[env->var_count] = strdup(name);
    // 增加引用计数，因为它被环境持有
    if (val.type == VAL_STRING || val.type == VAL_OBJECT) {
        gc_inc_ref(val.data.obj);
    }
    env->values[env->var_count] = val;
    env->var_count++;
}

// --------------- 虚拟机核心操作 ---------------
// 创建新环境
Env* create_env(Env* parent) {
    Env* env = malloc(sizeof(Env));
    if (!env) {
        fprintf(stderr, "内存分配失败！\n");
        exit(1);
    }
    env->var_count = 0;
    env->parent = parent;
    return env;
}

// 释放环境
void free_env(Env* env) {
    for (int i = 0; i < env->var_count; i++) {
        free(env->names[i]);
        val_free(env->values[i]);
    }
    free(env);
}

void vm_init(StackVM* vm) {
    vm->sp = 0;
    vm->call_sp = 0;
    vm->current_env = create_env(NULL); // 创建全局环境
}

void vm_push(StackVM* vm, Value val) {
    if (vm->sp >= 64) {fprintf(stderr, "栈溢出！\n"); exit(1);}
    // 增加引用计数，因为值现在被栈持有
    if (val.type == VAL_STRING || val.type == VAL_OBJECT) {
        gc_inc_ref(val.data.obj);
    }
    vm->stack[vm->sp++] = val;
}

Value vm_pop(StackVM* vm) {
    if (vm->sp <= 0) {fprintf(stderr, "栈下溢！\n"); exit(1);}
    return vm->stack[--vm->sp];
}

// 直接从栈中弹出并释放值（用于不需要返回值的情况）
void vm_pop_free(StackVM* vm) {
    Value val = vm_pop(vm);
    val_free(val);
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
            // 压入布尔值：后续1字节表示布尔值
            case OP_PUSH_BOOL: {
                bool b = bytecode[ip++];
                vm_push(vm, val_boolean(b));
                break;
            }
            // 压入undefined
            case OP_PUSH_UNDEFINED: {
                vm_push(vm, val_undefined());
                break;
            }
            // 压入null
            case OP_PUSH_NULL: {
                vm_push(vm, val_null());
                break;
            }
            // 创建新对象
            case OP_NEW_OBJECT: {
                vm_push(vm, val_object());
                break;
            }
            // 设置对象属性：栈顶是值，栈次顶是对象，后续是属性名
            case OP_SET_PROP: {
                // 读取属性名
                uint8_t prop_len = bytecode[ip++];
                char prop_name[prop_len + 1];
                memcpy(prop_name, &bytecode[ip], prop_len);
                prop_name[prop_len] = '\0';
                ip += prop_len;
                
                // 弹出栈顶值（属性值）和对象
                Value value = vm_pop(vm);
                Value obj_val = vm_pop(vm);
                
                if (obj_val.type != VAL_OBJECT) {
                    fprintf(stderr, "设置属性的目标不是对象！\n");
                    exit(1);
                }
                
                Object* obj = (Object*)obj_val.data.obj;
                
                // 查找属性是否已存在
                int index = -1;
                for (int i = 0; i < obj->property_count; i++) {
                    if (strcmp(obj->prop_names[i], prop_name) == 0) {
                        index = i;
                        break;
                    }
                }
                
                if (index == -1) {
                    // 添加新属性
                    if (obj->property_count >= MAX_PROPS) {
                        fprintf(stderr, "对象属性数量超限！\n");
                        exit(1);
                    }
                    
                    obj->prop_names = realloc(obj->prop_names, (obj->property_count + 1) * sizeof(uint8_t*));
                    obj->prop_values = realloc(obj->prop_values, (obj->property_count + 1) * sizeof(Value));
                    
                    obj->prop_names[obj->property_count] = strdup(prop_name);
                    
                    // 增加引用计数，因为对象现在持有这个值
                    if (value.type == VAL_STRING || value.type == VAL_OBJECT) {
                        gc_inc_ref(value.data.obj);
                    }
                    
                    obj->prop_values[obj->property_count] = value;
                    obj->property_count++;
                } else {
                    // 更新现有属性
                    // 先释放旧值
                    val_free(obj->prop_values[index]);
                    
                    // 增加新值的引用计数
                    if (value.type == VAL_STRING || value.type == VAL_OBJECT) {
                        gc_inc_ref(value.data.obj);
                    }
                    
                    obj->prop_values[index] = value;
                }
                
                // 将对象重新压回栈顶
                vm_push(vm, obj_val);
                break;
            }
            // 获取对象属性：栈顶是对象，后续是属性名
            case OP_GET_PROP: {
                // 读取属性名
                uint8_t prop_len = bytecode[ip++];
                char prop_name[prop_len + 1];
                memcpy(prop_name, &bytecode[ip], prop_len);
                prop_name[prop_len] = '\0';
                ip += prop_len;
                
                // 弹出栈顶对象
                Value obj_val = vm_pop(vm);
                
                if (obj_val.type != VAL_OBJECT) {
                    fprintf(stderr, "获取属性的目标不是对象！\n");
                    exit(1);
                }
                
                Object* obj = (Object*)obj_val.data.obj;
                
                // 查找属性
                Value result = val_undefined();
                for (int i = 0; i < obj->property_count; i++) {
                    if (strcmp(obj->prop_names[i], prop_name) == 0) {
                        result = obj->prop_values[i];
                        // 增加引用计数，因为我们将返回这个值的引用
                        if (result.type == VAL_STRING || result.type == VAL_OBJECT) {
                            gc_inc_ref(result.data.obj);
                        }
                        break;
                    }
                }
                
                // 释放对象引用
                val_free(obj_val);
                
                // 将属性值压入栈顶
                vm_push(vm, result);
                break;
            }
            // 压入变量：1字节长度 + N字节变量名
            case OP_PUSH_VAR: {
                uint8_t name_len = bytecode[ip++];
                char name[name_len + 1];
                memcpy(name, &bytecode[ip], name_len);
                name[name_len] = '\0';
                Value val = env_get(vm->current_env, name);
                if (val.type == VAL_UNDEFINED) {
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
                env_set(vm->current_env, name, val);
                ip += name_len;
                break;
            }
            // 创建新作用域
            case OP_PUSH_ENV: {
                // 创建新环境，将当前环境作为父环境
                vm->current_env = create_env(vm->current_env);
                break;
            }
            // 退出当前作用域
            case OP_POP_ENV: {
                Env *old_env = vm->current_env;
                vm->current_env = vm->current_env->parent;
                // 释放当前环境
                free_env(old_env);
                break;
            }
            // 加法：支持数值+数值、字符串+字符串、字符串+数值（类似 JS 隐式转换）
            case OP_ADD: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (a.type == VAL_NUMBER && b.type == VAL_NUMBER) {
                    vm_push(vm, val_number(a.data.number + b.data.number));
                } else if (a.type == VAL_STRING || b.type == VAL_STRING) {
                    // 任何一方为字符串，都将另一方转换为字符串后拼接
                    char* str_a;
                    size_t len_a;
                    char* str_b;
                    size_t len_b;
                    
                    // 转换a为字符串
                    if (a.type == VAL_STRING) {
                        StringObject* sobj_a = (StringObject*)a.data.obj;
                        len_a = sobj_a->length;
                        str_a = sobj_a->chars;
                    } else if (a.type == VAL_NUMBER) {
                        char num_str[32];
                        sprintf(num_str, "%.2f", a.data.number);
                        len_a = strlen(num_str);
                        str_a = num_str;
                    } else if (a.type == VAL_BOOLEAN) {
                        str_a = a.data.boolean ? "true" : "false";
                        len_a = strlen(str_a);
                    } else if (a.type == VAL_UNDEFINED) {
                        str_a = "undefined";
                        len_a = strlen(str_a);
                    } else if (a.type == VAL_NULL) {
                        str_a = "null";
                        len_a = strlen(str_a);
                    } else {
                        str_a = "[object Object]";
                        len_a = strlen(str_a);
                    }
                    
                    // 转换b为字符串
                    if (b.type == VAL_STRING) {
                        StringObject* sobj_b = (StringObject*)b.data.obj;
                        len_b = sobj_b->length;
                        str_b = sobj_b->chars;
                    } else if (b.type == VAL_NUMBER) {
                        char num_str[32];
                        sprintf(num_str, "%.2f", b.data.number);
                        len_b = strlen(num_str);
                        str_b = num_str;
                    } else if (b.type == VAL_BOOLEAN) {
                        str_b = b.data.boolean ? "true" : "false";
                        len_b = strlen(str_b);
                    } else if (b.type == VAL_UNDEFINED) {
                        str_b = "undefined";
                        len_b = strlen(str_b);
                    } else if (b.type == VAL_NULL) {
                        str_b = "null";
                        len_b = strlen(str_b);
                    } else {
                        str_b = "[object Object]";
                        len_b = strlen(str_b);
                    }
                    
                    // 拼接字符串
                    size_t total_len = len_a + len_b;
                    char* new_chars = malloc(total_len + 1);
                    strcpy(new_chars, str_a);
                    strcat(new_chars, str_b);
                    
                    // 创建新字符串对象
                    StringObject* new_str = (StringObject*)create_object(VAL_STRING, sizeof(StringObject));
                    new_str->length = total_len;
                    new_str->chars = new_chars;
                    
                    Value res = {.type = VAL_STRING, .data.obj = (ObjectHeader*)new_str};
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
                memcpy(&func_offset, &bytecode[ip + 1], sizeof(int));
                ip += sizeof(int) + 1; // +1 是跳过 OP_CALL 指令本身
                vm_call(vm, ip); // 保存当前 ip（函数执行完后返回此处）
                ip = func_offset; // 跳转到函数起始位置
                break;
            }
            // 函数返回：恢复 ip 到调用前位置，保持返回值在栈上
            case OP_RET: {
                ip = vm_ret(vm);
                break;
            }
            // 打印：支持多类型输出
            case OP_PRINT: {
                Value val = vm_pop(vm);
                switch (val.type) {
                    case VAL_NUMBER: 
                        printf("输出：%g\n", val.data.number); 
                        break;
                    case VAL_STRING: {
                        StringObject* str_obj = (StringObject*)val.data.obj;
                        printf("输出：%s\n", str_obj->chars); 
                        break;
                    }
                    case VAL_BOOLEAN: 
                        printf("输出：%s\n", val.data.boolean ? "true" : "false"); 
                        break;
                    case VAL_UNDEFINED: 
                        printf("输出：undefined\n"); 
                        break;
                    case VAL_NULL: 
                        printf("输出：null\n"); 
                        break;
                    case VAL_OBJECT: 
                        printf("输出：[object Object]\n"); 
                        break;
                    default:
                        printf("输出：未知类型\n"); 
                        exit(1);
                        break;
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

// 测试：执行「变量赋值 + 函数调用 + 字符串拼接 + 数值运算 + 新类型测试」
int main() {
    StackVM vm;
    vm_init(&vm);

    // 测试作用域功能
    uint8_t bytecode[] = {
        // 全局作用域：定义全局变量
        OP_PUSH_STR, 4, 'x','=','1','0',                    // 压入字符串 "10"
        OP_STORE_VAR, 1, 'x',                       // 存储到全局变量 x = "10"
        
        OP_PUSH_STR, 8,'s','=','g','l','o','b','a','l',    // 压入字符串 "global"
        OP_STORE_VAR, 1, 's',                       // 存储到全局变量 s = "global"
        
        // 打印全局变量
        OP_PUSH_VAR, 1, 'x',                        // 加载全局变量 x
        OP_PRINT,                                   // 打印 x (预期: 10)
        
        OP_PUSH_VAR, 1, 's',                        // 加载全局变量 s
        OP_PRINT,                                   // 打印 s (预期: global)
        
        // 创建局部作用域
        OP_PUSH_ENV,                                // 进入局部作用域
        
        // 局部作用域：定义局部变量，覆盖全局变量
        OP_PUSH_STR, 4, 'x','=','2','0',                    // 压入字符串 "20"
        OP_STORE_VAR, 1, 'x',                       // 存储到局部变量 x = "20"
        
        OP_PUSH_STR, 7, 'y', '=','l','o','c','a','l',        // 压入字符串 "local"
        OP_STORE_VAR, 1, 'y',                       // 存储到局部变量 y = "local"
        
        // 打印局部变量和全局变量
        OP_PUSH_VAR, 1, 'x',                        // 加载局部变量 x
        OP_PRINT,                                   // 打印 x (预期: 20)
        
        OP_PUSH_VAR, 1, 'y',                        // 加载局部变量 y
        OP_PRINT,                                   // 打印 y (预期: local)
        
        OP_PUSH_VAR, 1, 's',                        // 加载全局变量 s (从作用域链查找)
        OP_PRINT,                                   // 打印 s (预期: global)
        
        // 退出局部作用域
        OP_POP_ENV,                                 // 退出局部作用域
        
        // 验证变量恢复到全局作用域
        OP_PUSH_VAR, 1, 'x',                        // 加载全局变量 x
        OP_PRINT,                                   // 打印 x (预期: 10)
        
        // 验证局部变量y已不存在
        OP_PUSH_VAR, 1, 'y',                        // 尝试加载局部变量 y
        OP_PRINT,                                   // 这里应该报错，因为y已不存在
        
        OP_EXIT                                     // 退出程序
    };
    
    vm_execute(&vm, bytecode, sizeof(bytecode));
    
    // 释放全局环境内存
    free_env(vm.current_env);
    return 0;
}