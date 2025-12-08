#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// 导入虚拟机的指令枚举
#include "stack-vm.h"

// 编译器的常量定义
#define MAX_TOKEN_LEN 64
#define MAX_BYTECODE_LEN 512

// 词法分析器的标记类型
typedef enum {
    TOKEN_EOF,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_BOOLEAN,
    TOKEN_UNDEFINED,
    TOKEN_NULL,
    TOKEN_IDENTIFIER,
    TOKEN_OPERATOR,
    TOKEN_PUNCTUATOR,
    TOKEN_KEYWORD
} TokenType;

// 关键字枚举
typedef enum {
    KW_VAR,
    KW_PRINT,
    KW_FUNCTION,
    KW_RETURN
} Keyword;

// 标记结构体
typedef struct {
    TokenType type;
    char lexeme[MAX_TOKEN_LEN];
    int line;
    int col;
} Token;

// 词法分析器结构体
typedef struct {
    const char* source;
    int pos;
    int line;
    int col;
    Token current;
} Lexer;

// 语法分析器结构体
typedef struct {
    Lexer* lexer;
    uint8_t bytecode[MAX_BYTECODE_LEN];
    int bc_pos;
} Parser;

// 辅助函数：判断字符是否为空白字符
bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// 辅助函数：判断字符是否为字母或下划线
bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

// 辅助函数：判断字符是否为数字
bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

// 辅助函数：判断字符是否为字母、数字或下划线
bool is_alnum(char c) {
    return is_alpha(c) || is_digit(c) || c == '_' || c == '$';
}

// 辅助函数：检查是否为关键字
int is_keyword(const char* lexeme) {
    if (strcmp(lexeme, "var") == 0) return KW_VAR;
    if (strcmp(lexeme, "print") == 0) return KW_PRINT;
    if (strcmp(lexeme, "function") == 0) return KW_FUNCTION;
    if (strcmp(lexeme, "return") == 0) return KW_RETURN;
    return -1;
}

// 初始化词法分析器
void lexer_init(Lexer* lexer, const char* source) {
    lexer->source = source;
    lexer->pos = 0;
    lexer->line = 1;
    lexer->col = 1;
}

// 获取下一个字符
char lexer_peek(Lexer* lexer) {
    return lexer->source[lexer->pos];
}

// 消费当前字符
char lexer_consume(Lexer* lexer) {
    char c = lexer->source[lexer->pos++];
    if (c == '\n') {
        lexer->line++;
        lexer->col = 1;
    } else {
        lexer->col++;
    }
    return c;
}

// 匹配并消费特定字符
bool lexer_match(Lexer* lexer, char expected) {
    if (lexer_peek(lexer) == expected) {
        lexer_consume(lexer);
        return true;
    }
    return false;
}

// 跳过空白字符和注释
void lexer_skip_whitespace(Lexer* lexer) {
    while (true) {
        // 跳过空白字符
        while (is_whitespace(lexer_peek(lexer))) {
            lexer_consume(lexer);
        }
        
        // 检查是否为注释
        if (lexer_peek(lexer) == '/') {
            lexer_consume(lexer); // 消费 '/' 
            
            if (lexer_peek(lexer) == '/') {
                // 单行注释，跳过到行尾
                while (lexer_peek(lexer) != '\n' && lexer_peek(lexer) != '\0') {
                    lexer_consume(lexer);
                }
            } else if (lexer_peek(lexer) == '*') {
                // 多行注释，跳过到 */
                lexer_consume(lexer); // 消费 '*'
                while (true) {
                    if (lexer_peek(lexer) == '\0') {
                        // 遇到文件末尾，退出注释处理
                        break;
                    } else if (lexer_peek(lexer) == '*') {
                        lexer_consume(lexer); // 消费 '*'
                        if (lexer_peek(lexer) == '/') {
                            lexer_consume(lexer); // 消费 '/'
                            break; // 注释结束
                        }
                    } else {
                        lexer_consume(lexer);
                    }
                }
            } else {
                // 不是注释，将 '/' 放回去
                lexer->pos--;
                lexer->col--;
                break;
            }
        } else {
            break; // 不是空白也不是注释，退出循环
        }
    }
}

// 解析数字
void lexer_number(Lexer* lexer, Token* token) {
    int start = lexer->pos;
    while (is_digit(lexer_peek(lexer)) || lexer_peek(lexer) == '.') {
        lexer_consume(lexer);
    }
    int len = lexer->pos - start;
    strncpy(token->lexeme, &lexer->source[start], len);
    token->lexeme[len] = '\0';
    token->type = TOKEN_NUMBER;
}

// 解析字符串
void lexer_string(Lexer* lexer, Token* token) {
    lexer_consume(lexer); // 跳过开头的引号
    int start = lexer->pos;
    while (lexer_peek(lexer) != '"' && lexer_peek(lexer) != '\0') {
        lexer_consume(lexer);
    }
    int len = lexer->pos - start;
    strncpy(token->lexeme, &lexer->source[start], len);
    token->lexeme[len] = '\0';
    token->type = TOKEN_STRING;
    lexer_consume(lexer); // 跳过结尾的引号
}

// 解析标识符或关键字
void lexer_identifier(Lexer* lexer, Token* token) {
    int start = lexer->pos;
    while (is_alnum(lexer_peek(lexer))) {
        lexer_consume(lexer);
    }
    int len = lexer->pos - start;
    strncpy(token->lexeme, &lexer->source[start], len);
    token->lexeme[len] = '\0';
    
    int keyword = is_keyword(token->lexeme);
    if (keyword != -1) {
        token->type = TOKEN_KEYWORD;
    } else {
        token->type = TOKEN_IDENTIFIER;
    }
}

// 获取下一个标记
void lexer_next_token(Lexer* lexer, Token* token) {
    token->line = lexer->line;
    token->col = lexer->col;
    
    lexer_skip_whitespace(lexer);
    
    char c = lexer_peek(lexer);
    
    switch (c) {
        case '\0':
            token->type = TOKEN_EOF;
            token->lexeme[0] = '\0';
            break;
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            lexer_number(lexer, token);
            break;
        case '"':
            lexer_string(lexer, token);
            break;
        case '+': case '-': case '*': case '/':
            token->type = TOKEN_OPERATOR;
            token->lexeme[0] = lexer_consume(lexer);
            token->lexeme[1] = '\0';
            break;
        case '=': case ';': case '(': case ')': case '{': case '}':
        case '.': case ':': case ',':
            token->type = TOKEN_PUNCTUATOR;
            token->lexeme[0] = lexer_consume(lexer);
            token->lexeme[1] = '\0';
            break;
        default:
            if (c == '\0') {
                token->type = TOKEN_EOF;
                token->lexeme[0] = '\0';
            } else if (is_alpha(c) || c == '_' || c == '$') {
                lexer_identifier(lexer, token);
                // 检查是否为布尔值、undefined或null
                if (strcmp(token->lexeme, "true") == 0 || strcmp(token->lexeme, "false") == 0) {
                    token->type = TOKEN_BOOLEAN;
                } else if (strcmp(token->lexeme, "undefined") == 0) {
                    token->type = TOKEN_UNDEFINED;
                } else if (strcmp(token->lexeme, "null") == 0) {
                    token->type = TOKEN_NULL;
                }
            } else {
                // 未知字符，简单消费它
                token->type = TOKEN_OPERATOR;
                token->lexeme[0] = lexer_consume(lexer);
                token->lexeme[1] = '\0';
            }
            break;
    }
}

// 初始化语法分析器
void parser_init(Parser* parser, Lexer* lexer) {
    parser->lexer = lexer;
    parser->bc_pos = 0;
    // 预读第一个标记
    lexer_next_token(lexer, &parser->lexer->current);
}

// 检查当前标记类型
bool parser_check(Parser* parser, TokenType type) {
    return parser->lexer->current.type == type;
}

// 检查当前标记是否为特定关键字
bool parser_check_keyword(Parser* parser, const char* keyword) {
    return parser->lexer->current.type == TOKEN_KEYWORD && 
           strcmp(parser->lexer->current.lexeme, keyword) == 0;
}

// 匹配并消费特定类型的标记
bool parser_match(Parser* parser, TokenType type) {
    if (parser_check(parser, type)) {
        lexer_next_token(parser->lexer, &parser->lexer->current);
        return true;
    }
    return false;
}

// 匹配并消费特定关键字
bool parser_match_keyword(Parser* parser, const char* keyword) {
    if (parser_check_keyword(parser, keyword)) {
        lexer_next_token(parser->lexer, &parser->lexer->current);
        return true;
    }
    return false;
}

// 生成字节码：添加一个字节
void emit_byte(Parser* parser, uint8_t byte) {
    if (parser->bc_pos >= MAX_BYTECODE_LEN) {
        fprintf(stderr, "字节码超出最大长度限制！\n");
        exit(1);
    }
    parser->bytecode[parser->bc_pos++] = byte;
}

// 生成字节码：添加一个双精度浮点数
void emit_double(Parser* parser, double value) {
    if (parser->bc_pos + sizeof(double) > MAX_BYTECODE_LEN) {
        fprintf(stderr, "字节码超出最大长度限制！\n");
        exit(1);
    }
    memcpy(&parser->bytecode[parser->bc_pos], &value, sizeof(double));
    parser->bc_pos += sizeof(double);
}

// 生成字节码：添加一个字符串
void emit_string(Parser* parser, const char* str) {
    int len = strlen(str);
    if (len > 255) {
        fprintf(stderr, "字符串太长，超过255字节限制！\n");
        exit(1);
    }
    if (parser->bc_pos + 1 + len > MAX_BYTECODE_LEN) {
        fprintf(stderr, "字节码超出最大长度限制！\n");
        exit(1);
    }
    // 先添加字符串长度
    emit_byte(parser, (uint8_t)len);
    // 再添加字符串内容
    memcpy(&parser->bytecode[parser->bc_pos], str, len);
    parser->bc_pos += len;
}

// 解析表达式（简单的加减表达式）
void parse_expression(Parser* parser);

// 解析主表达式（数字、字符串、标识符等）
void parse_primary(Parser* parser) {
    if (parser_check(parser, TOKEN_NUMBER)) {
        // 生成 OP_PUSH_NUM 指令
        emit_byte(parser, OP_PUSH_NUM);
        double num = atof(parser->lexer->current.lexeme);
        emit_double(parser, num);
        parser_match(parser, TOKEN_NUMBER);
    } else if (parser_check(parser, TOKEN_STRING)) {
        // 生成 OP_PUSH_STR 指令
        emit_byte(parser, OP_PUSH_STR);
        emit_string(parser, parser->lexer->current.lexeme);
        parser_match(parser, TOKEN_STRING);
    } else if (parser_check(parser, TOKEN_BOOLEAN)) {
        // 生成 OP_PUSH_BOOL 指令
        emit_byte(parser, OP_PUSH_BOOL);
        bool value = strcmp(parser->lexer->current.lexeme, "true") == 0;
        emit_byte(parser, value ? 1 : 0);
        parser_match(parser, TOKEN_BOOLEAN);
    } else if (parser_check(parser, TOKEN_UNDEFINED)) {
        // 生成 OP_PUSH_UNDEFINED 指令
        emit_byte(parser, OP_PUSH_UNDEFINED);
        parser_match(parser, TOKEN_UNDEFINED);
    } else if (parser_check(parser, TOKEN_NULL)) {
        // 生成 OP_PUSH_NULL 指令
        emit_byte(parser, OP_PUSH_NULL);
        parser_match(parser, TOKEN_NULL);
    } else if (parser_check(parser, TOKEN_IDENTIFIER)) {
        // 生成 OP_PUSH_VAR 指令
        emit_byte(parser, OP_PUSH_VAR);
        emit_string(parser, parser->lexer->current.lexeme);
        parser_match(parser, TOKEN_IDENTIFIER);
        
        // 检查是否为属性访问（如 obj.prop）
        while (parser_check(parser, TOKEN_PUNCTUATOR) && 
               parser->lexer->current.lexeme[0] == '.') {
            parser_match(parser, TOKEN_PUNCTUATOR); // 消费 '.'
            if (parser_check(parser, TOKEN_IDENTIFIER)) {
                // 生成 OP_GET_PROP 指令
                emit_byte(parser, OP_GET_PROP);
                emit_string(parser, parser->lexer->current.lexeme);
                parser_match(parser, TOKEN_IDENTIFIER);
            } else {
                fprintf(stderr, "错误：属性名必须是标识符\n");
                exit(1);
            }
        }
    } else if (parser_check(parser, TOKEN_PUNCTUATOR) && 
               parser->lexer->current.lexeme[0] == '(') {
        // 括号表达式
        parser_match(parser, TOKEN_PUNCTUATOR);
        parse_expression(parser);
        if (!parser_match(parser, TOKEN_PUNCTUATOR) || 
            parser->lexer->current.lexeme[0] != ')') {
            fprintf(stderr, "错误：缺少右括号\n");
            exit(1);
        }
    } else if (parser_check(parser, TOKEN_PUNCTUATOR) && 
               parser->lexer->current.lexeme[0] == '{') {
        // 对象创建表达式 {}
        parser_match(parser, TOKEN_PUNCTUATOR); // 消费 '{'
        
        // 生成 OP_NEW_OBJECT 指令
        emit_byte(parser, OP_NEW_OBJECT);
        
        // 解析属性列表
        if (parser_check(parser, TOKEN_PUNCTUATOR) && 
            parser->lexer->current.lexeme[0] == '}') {
            // 空对象 {}
            parser_match(parser, TOKEN_PUNCTUATOR); // 消费 '}'
        } else {
            // 至少有一个属性
            while (1) {
                // 解析属性名
                if (parser_check(parser, TOKEN_IDENTIFIER)) {
                    char prop_name[MAX_TOKEN_LEN];
                    strcpy(prop_name, parser->lexer->current.lexeme);
                    parser_match(parser, TOKEN_IDENTIFIER);
                    
                    // 消费 ':'
                    if (!parser_check(parser, TOKEN_PUNCTUATOR) || 
                        parser->lexer->current.lexeme[0] != ':') {
                        fprintf(stderr, "错误：对象属性缺少冒号，当前标记: %s\n", parser->lexer->current.lexeme);
                        exit(1);
                    }
                    parser_match(parser, TOKEN_PUNCTUATOR);
                    
                    // 解析属性值
                    parse_expression(parser);
                    
                    // 生成 OP_SET_PROP 指令
                    emit_byte(parser, OP_SET_PROP);
                    emit_string(parser, prop_name);
                    
                    // 检查是否为对象结束
                    if (parser_check(parser, TOKEN_PUNCTUATOR) && 
                        parser->lexer->current.lexeme[0] == '}') {
                        parser_match(parser, TOKEN_PUNCTUATOR); // 消费 '}'
                        break;
                    }
                    
                    // 检查是否有逗号分隔符
                    if (parser_check(parser, TOKEN_PUNCTUATOR) && 
                        parser->lexer->current.lexeme[0] == ',') {
                        parser_match(parser, TOKEN_PUNCTUATOR); // 消费 ','
                        // 继续下一个属性
                    } else {
                        fprintf(stderr, "错误：对象属性列表格式错误，当前标记: %s\n", parser->lexer->current.lexeme);
                        exit(1);
                    }
                } else {
                    fprintf(stderr, "错误：对象属性名必须是标识符，当前标记: %s\n", parser->lexer->current.lexeme);
                    exit(1);
                }
            }
        }
    } else {
        fprintf(stderr, "错误：无法解析的表达式\n");
        exit(1);
    }
}

// 解析表达式（简单的加减表达式）
void parse_expression(Parser* parser) {
    parse_primary(parser);
    
    while (parser_check(parser, TOKEN_OPERATOR)) {
        // 确保运算符标记有有效内容
        if (strlen(parser->lexer->current.lexeme) == 0) {
            fprintf(stderr, "错误：无效的运算符标记\n");
            exit(1);
        }
        
        char op = parser->lexer->current.lexeme[0];
        parser_match(parser, TOKEN_OPERATOR);
        
        parse_primary(parser);
        
        // 生成对应的运算符指令
        if (op == '+') {
            emit_byte(parser, OP_ADD);
        } else {
            fprintf(stderr, "错误：不支持的运算符 '%c'\n", op);
            exit(1);
        }
    }
}

// 解析赋值语句
void parse_assignment(Parser* parser) {
    if (parser_check(parser, TOKEN_IDENTIFIER)) {
        // 保存变量名
        char var_name[MAX_TOKEN_LEN];
        strcpy(var_name, parser->lexer->current.lexeme);
        parser_match(parser, TOKEN_IDENTIFIER);
        
        // 检查是否为属性赋值（如 obj.prop = value）
        if (parser_check(parser, TOKEN_PUNCTUATOR) && 
            parser->lexer->current.lexeme[0] == '.') {
            parser_match(parser, TOKEN_PUNCTUATOR); // 消费 '.'
            if (parser_check(parser, TOKEN_IDENTIFIER)) {
                char prop_name[MAX_TOKEN_LEN];
                strcpy(prop_name, parser->lexer->current.lexeme);
                parser_match(parser, TOKEN_IDENTIFIER);
                
                if (parser_check(parser, TOKEN_PUNCTUATOR) && 
                    parser->lexer->current.lexeme[0] == '=') {
                    parser_match(parser, TOKEN_PUNCTUATOR); // 消费 '='
                    
                    // 首先加载对象到栈上
                    emit_byte(parser, OP_PUSH_VAR);
                    emit_string(parser, var_name);
                    
                    // 然后解析赋值表达式（值）
                    parse_expression(parser);
                    
                    // 生成 OP_SET_PROP 指令
                    emit_byte(parser, OP_SET_PROP);
                    emit_string(parser, prop_name);
                } else {
                    // 不是赋值，而是普通的属性访问
                    emit_byte(parser, OP_PUSH_VAR);
                    emit_string(parser, var_name);
                    emit_byte(parser, OP_GET_PROP);
                    emit_string(parser, prop_name);
                }
            } else {
                fprintf(stderr, "错误：属性名必须是标识符\n");
                exit(1);
            }
        } else if (parser_check(parser, TOKEN_PUNCTUATOR) && 
                   parser->lexer->current.lexeme[0] == '=') {
            parser_match(parser, TOKEN_PUNCTUATOR); // 消费 '='
            
            // 解析赋值表达式
            parse_expression(parser);
            
            // 生成 OP_STORE_VAR 指令
            emit_byte(parser, OP_STORE_VAR);
            emit_string(parser, var_name);
        } else {
            // 不是赋值，而是普通的标识符
            emit_byte(parser, OP_PUSH_VAR);
            emit_string(parser, var_name);
        }
    }
}

// 解析变量声明语句（var x = 10;）
void parse_var_declaration(Parser* parser) {
    // 消费 var 关键字
    parser_match_keyword(parser, "var");
    
    if (parser_check(parser, TOKEN_IDENTIFIER)) {
        // 保存变量名
        char var_name[MAX_TOKEN_LEN];
        strcpy(var_name, parser->lexer->current.lexeme);
        parser_match(parser, TOKEN_IDENTIFIER);
        
        if (parser_check(parser, TOKEN_PUNCTUATOR) && 
            parser->lexer->current.lexeme[0] == '=') {
            parser_match(parser, TOKEN_PUNCTUATOR); // 消费 '='
            
            // 解析初始化表达式
            parse_expression(parser);
            
            // 生成 OP_STORE_VAR 指令
            emit_byte(parser, OP_STORE_VAR);
            emit_string(parser, var_name);
        } else {
            // 没有初始化值，默认为 undefined
            emit_byte(parser, OP_PUSH_UNDEFINED);
            emit_byte(parser, OP_STORE_VAR);
            emit_string(parser, var_name);
        }
    } else {
        fprintf(stderr, "错误：变量声明缺少标识符\n");
        exit(1);
    }
}

// 解析打印语句（print(x);）
void parse_print_statement(Parser* parser) {
    // 消费 print 关键字
    parser_match_keyword(parser, "print");
    
    if (parser_check(parser, TOKEN_PUNCTUATOR) && 
        parser->lexer->current.lexeme[0] == '(') {
        parser_match(parser, TOKEN_PUNCTUATOR); // 消费 '('
        
        // 解析要打印的表达式
        parse_expression(parser);
        
        // 检查是否有右括号
        if (!parser_check(parser, TOKEN_PUNCTUATOR) || 
            parser->lexer->current.lexeme[0] != ')') {
            fprintf(stderr, "错误：print 语句缺少右括号\n");
            exit(1);
        }
        parser_match(parser, TOKEN_PUNCTUATOR); // 消费 ')'
        
        // 生成 OP_PRINT 指令
        emit_byte(parser, OP_PRINT);
    } else {
        fprintf(stderr, "错误：print 语句缺少左括号\n");
        exit(1);
    }
}

// 解析语句块（{ ... }）
void parse_block(Parser* parser) {
    if (parser_check(parser, TOKEN_PUNCTUATOR) && 
        parser->lexer->current.lexeme[0] == '{') {
        parser_match(parser, TOKEN_PUNCTUATOR); // 消费 '{'
        
        // 生成 OP_PUSH_ENV 指令，创建新的作用域
        emit_byte(parser, OP_PUSH_ENV);
        
        // 解析块内的语句
        while (!parser_check(parser, TOKEN_PUNCTUATOR) || 
               parser->lexer->current.lexeme[0] != '}') {
            if (parser_check(parser, TOKEN_KEYWORD)) {
                if (parser_check_keyword(parser, "var")) {
                    parse_var_declaration(parser);
                } else if (parser_check_keyword(parser, "print")) {
                    parse_print_statement(parser);
                } else {
                    fprintf(stderr, "错误：未知的关键字 '%s'\n", parser->lexer->current.lexeme);
                    exit(1);
                }
            } else {
                // 可能是赋值语句
                parse_assignment(parser);
            }
            
            // 消费分号
            parser_match(parser, TOKEN_PUNCTUATOR);
        }
        
        // 生成 OP_POP_ENV 指令，退出当前作用域
        emit_byte(parser, OP_POP_ENV);
        
        parser_match(parser, TOKEN_PUNCTUATOR); // 消费 '}'
    }
}

// 解析语句
void parse_statement(Parser* parser) {
    if (parser_check(parser, TOKEN_KEYWORD)) {
        if (parser_check_keyword(parser, "var")) {
            parse_var_declaration(parser);
        } else if (parser_check_keyword(parser, "print")) {
            parse_print_statement(parser);
        } else {
            fprintf(stderr, "错误：未知的关键字 '%s'\n", parser->lexer->current.lexeme);
            exit(1);
        }
    } else if (parser_check(parser, TOKEN_PUNCTUATOR) && 
               parser->lexer->current.lexeme[0] == '{') {
        parse_block(parser);
    } else {
        // 可能是赋值语句
        parse_assignment(parser);
    }
}

// 解析程序（多个语句）
void parse_program(Parser* parser) {
    while (!parser_check(parser, TOKEN_EOF)) {
        parse_statement(parser);
        // 消费分号（如果有）
        parser_match(parser, TOKEN_PUNCTUATOR);
    }
    
    // 最后添加 OP_EXIT 指令
    emit_byte(parser, OP_EXIT);
}

// 编译函数
uint8_t* compile(const char* source, int* bytecode_len) {
    Lexer lexer;
    Parser parser;
    
    // 初始化词法分析器
    lexer_init(&lexer, source);
    
    // 初始化语法分析器
    parser_init(&parser, &lexer);
    
    // 解析并生成字节码
    parse_program(&parser);
    
    // 返回生成的字节码
    uint8_t* bytecode = malloc(parser.bc_pos);
    if (!bytecode) {
        fprintf(stderr, "内存分配失败！\n");
        exit(1);
    }
    memcpy(bytecode, parser.bytecode, parser.bc_pos);
    *bytecode_len = parser.bc_pos;
    
    return bytecode;
}

// 打印帮助信息
void print_help() {
    printf("stack-vm-compiler - 将 Stack VM 源代码编译为字节码\n");
    printf("\n");
    printf("用法: stack-vm-compiler [选项] 输入文件 [输出文件]\n");
    printf("\n");
    printf("选项:\n");
    printf("  -h, --help      显示此帮助信息\n");
    printf("  -o <文件>       指定输出文件路径\n");
    printf("  -c              将字节码输出到标准输出\n");
    printf("  -e              直接执行编译后的字节码（不输出文件）\n");
    printf("\n");
    printf("示例:\n");
    printf("  stack-vm-compiler source.txt output.bin\n");
    printf("  stack-vm-compiler -o output.bin source.txt\n");
    printf("  stack-vm-compiler -c source.txt | hexdump -C\n");
    printf("  stack-vm-compiler -e source.txt\n");
}

// 读取文件内容
char* read_file(const char* filename, size_t* size) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "错误：无法打开文件 '%s'\n", filename);
        return NULL;
    }
    
    // 获取文件大小
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // 分配内存并读取文件内容
    char* buffer = (char*)malloc(*size + 1);
    if (!buffer) {
        fprintf(stderr, "错误：内存分配失败\n");
        fclose(file);
        return NULL;
    }
    
    size_t read = fread(buffer, 1, *size, file);
    if (read != *size) {
        fprintf(stderr, "错误：读取文件失败\n");
        free(buffer);
        fclose(file);
        return NULL;
    }
    
    buffer[*size] = '\0'; // 确保字符串以空字符结尾
    fclose(file);
    
    return buffer;
}

// 写入文件内容
bool write_file(const char* filename, const uint8_t* data, size_t size) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "错误：无法打开文件 '%s' 进行写入\n", filename);
        return false;
    }
    
    size_t written = fwrite(data, 1, size, file);
    if (written != size) {
        fprintf(stderr, "错误：写入文件失败\n");
        fclose(file);
        return false;
    }
    
    fclose(file);
    return true;
}

// 主函数：命令行工具入口
#ifdef COMPILER_TEST
int main(int argc, char* argv[]) {
    const char* input_file = NULL;
    const char* output_file = NULL;
    bool output_to_stdout = false;
    bool execute_only = false;
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_file = argv[++i];
            } else {
                fprintf(stderr, "错误：选项 '-o' 需要一个参数\n");
                print_help();
                return 1;
            }
        } else if (strcmp(argv[i], "-c") == 0) {
            output_to_stdout = true;
        } else if (strcmp(argv[i], "-e") == 0) {
            execute_only = true;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "错误：未知选项 '%s'\n", argv[i]);
            print_help();
            return 1;
        } else if (!input_file) {
            input_file = argv[i];
        } else if (!output_file) {
            output_file = argv[i];
        } else {
            fprintf(stderr, "错误：太多的参数\n");
            print_help();
            return 1;
        }
    }
    
    // 检查是否提供了输入文件
    if (!input_file) {
        fprintf(stderr, "错误：未提供输入文件\n");
        print_help();
        return 1;
    }
    
    // 读取输入文件
    size_t file_size;
    char* source_code = read_file(input_file, &file_size);
    if (!source_code) {
        return 1;
    }
    
    // 编译源代码
    int bytecode_len;
    uint8_t* bytecode = compile(source_code, &bytecode_len);
    
    // 释放源代码内存
    free(source_code);
    
    if (!bytecode) {
        fprintf(stderr, "错误：编译失败\n");
        return 1;
    }
    
    // 根据选项处理编译结果
    if (execute_only) {
        // 执行编译后的字节码
        StackVM vm;
        vm_init(&vm);
        vm_execute(&vm, bytecode, bytecode_len);
    } else {
        if (output_to_stdout) {
            // 输出到标准输出
            if ((int)fwrite(bytecode, 1, bytecode_len, stdout) != bytecode_len) {
                fprintf(stderr, "错误：写入标准输出失败\n");
                free(bytecode);
                return 1;
            }
        } else {
            // 写入输出文件
            if (!output_file) {
                // 如果没有指定输出文件，使用输入文件的基础名加上 .bin 扩展名
                char* base_name = strdup(input_file);
                char* dot = strrchr(base_name, '.');
                if (dot) {
                    *dot = '\0';
                }
                output_file = base_name;
                
                // 构造完整的输出文件名
                char* full_output_file = (char*)malloc(strlen(output_file) + 5);
                sprintf(full_output_file, "%s.bin", output_file);
                
                if (!write_file(full_output_file, bytecode, bytecode_len)) {
                    free(full_output_file);
                    free(base_name);
                    free(bytecode);
                    return 1;
                }
                
                printf("成功编译：%s -> %s\n", input_file, full_output_file);
                free(full_output_file);
                free(base_name);
            } else {
                // 使用指定的输出文件
                if (!write_file(output_file, bytecode, bytecode_len)) {
                    free(bytecode);
                    return 1;
                }
                
                printf("成功编译：%s -> %s\n", input_file, output_file);
            }
        }
    }
    
    // 释放字节码内存
    free(bytecode);
    
    return 0;
}
#endif