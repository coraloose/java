#ifndef SYMBOLS_H
#define SYMBOLS_H

#include "lexer.h"
#include "parser.h"

// define your own types and function prototypes for the symbol table(s) module below

// 四种标识符类别
typedef enum {
    STATIC_SYMBOL,  // static
    FIELD_SYMBOL,   // field
    ARG_SYMBOL,     // subroutine argument
    VAR_SYMBOL,     // local variable
    NONE_SYMBOL     // 未找到
} Kind;

// 初始化并清空整个符号表（类 + 子例程作用域）
void initSymbolTable(void);
// 进入新子例程时，清空子例程作用域（但保留类作用域）
void startSubroutine(void);

// 在当前作用域中定义一个新符号 name，类型为 type，类别为 kind；自动分配索引
void defineSymbol(const char *name, const char *type, Kind kind);
// 返回当前作用域中已定义的 kind 类别符号数
int varCount(Kind kind);
// 查询 name 的类别；如果不存在返回 NONE_SYMBOL
Kind kindOf(const char *name);
// 查询 name 的类型；如果不存在返回 NULL
const char* typeOf(const char *name);
// 查询 name 的索引；如果不存在返回 -1
int indexOf(const char *name);

#endif




