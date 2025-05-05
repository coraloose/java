
/************************************************************************
University of Leeds
School of Computing
COMP2932- Compiler Design and Construction
The Symbol Tables Module

I confirm that the following code has been developed and written by me and it is entirely the result of my own work.
I also confirm that I have not copied any parts of this program from another person or any other source or facilitated someone to copy this program from me.
I confirm that I will not publish the program online or share it with anyone without permission of the module leader.

Student Name:
Student ID:
Email:
Date Work Commenced:
*************************************************************************/

#include "symbols.h"
#include <string.h>
#include "symbols.h"

#define MAX_SYMBOLS 1024

// 符号描述
typedef struct {
    char name[64];   // 符号名
    char type[64];   // 符号类型（int/char/boolean 或 类名）
    Kind kind;       // 符号类别
    int index;       // 在该类别中的序号
} Symbol;

// 类作用域
static Symbol classTable[MAX_SYMBOLS];
static int classCount = 0;
static int classKindCount[4];  // 对应 STATIC_SYMBOL..VAR_SYMBOL

// 子例程作用域
static Symbol subTable[MAX_SYMBOLS];
static int subCount = 0;
static int subKindCount[4];

// 初始化整个符号表（包括类作用域和子例程作用域）
void initSymbolTable(void) {
    classCount = 0;
    memset(classKindCount, 0, sizeof(classKindCount));
    startSubroutine();
}

// 进入新子例程：仅清空子例程作用域，保留类作用域
void startSubroutine(void) {
    subCount = 0;
    memset(subKindCount, 0, sizeof(subKindCount));
}

// 定义一个新符号
void defineSymbol(const char *name, const char *type, Kind kind) {
    Symbol *s;
    int idx;
    switch (kind) {
        case STATIC_SYMBOL:
        case FIELD_SYMBOL:
            idx = classKindCount[kind]++;
            s = &classTable[classCount++];
            break;
        case ARG_SYMBOL:
        case VAR_SYMBOL:
            idx = subKindCount[kind]++;
            s = &subTable[subCount++];
            break;
        default:
            return; // NONE_SYMBOL 不定义
    }
    // 填充字段
    strncpy(s->name, name, sizeof(s->name)-1);
    s->name[sizeof(s->name)-1] = '\0';
    strncpy(s->type, type, sizeof(s->type)-1);
    s->type[sizeof(s->type)-1] = '\0';
    s->kind  = kind;
    s->index = idx;
}

// 返回已定义的同类符号数量
int varCount(Kind kind) {
    if (kind == STATIC_SYMBOL || kind == FIELD_SYMBOL)
        return classKindCount[kind];
    if (kind == ARG_SYMBOL || kind == VAR_SYMBOL)
        return subKindCount[kind];
    return 0;
}

// 查询符号的类别，优先子例程作用域
Kind kindOf(const char *name) {
    for (int i = subCount - 1; i >= 0; --i)
        if (!strcmp(subTable[i].name, name))
            return subTable[i].kind;
    for (int i = classCount - 1; i >= 0; --i)
        if (!strcmp(classTable[i].name, name))
            return classTable[i].kind;
    return NONE_SYMBOL;
}

// 查询符号的类型
const char* typeOf(const char *name) {
    for (int i = subCount - 1; i >= 0; --i)
        if (!strcmp(subTable[i].name, name))
            return subTable[i].type;
    for (int i = classCount - 1; i >= 0; --i)
        if (!strcmp(classTable[i].name, name))
            return classTable[i].type;
    return NULL;
}

// 查询符号的索引
int indexOf(const char *name) {
    for (int i = subCount - 1; i >= 0; --i)
        if (!strcmp(subTable[i].name, name))
            return subTable[i].index;
    for (int i = classCount - 1; i >= 0; --i)
        if (!strcmp(classTable[i].name, name))
            return classTable[i].index;
    return -1;
}