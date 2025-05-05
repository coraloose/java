// compiler.h
#ifndef COMPILER_H
#define COMPILER_H

#include "parser.h"

// 初始化编译器（符号表、代码生成等全局状态）
int InitCompiler(void);

// 编译目录 dir_name 下所有 .jack 文件
// 返回第一个出现的语法/语义错误信息，或 er==none 表示成功
ParserInfo compile(const char* dir_name);

// 编译结束，释放全局资源
int StopCompiler(void);

#endif  // COMPILER_H
