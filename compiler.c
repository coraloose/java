/************************************************************************
University of Leeds
School of Computing
COMP2932- Compiler Design and Construction
The Compiler Module

I confirm that the following code has been developed and written by me and it is entirely the result of my own work.
I also confirm that I have not copied any parts of this program from another person or any other source or facilitated someone to copy this program from me.
I confirm that I will not publish the program online or share it with anyone without permission of the module leader.

Student Name:
Student ID:
Email:
Date Work Commenced:
*************************************************************************/

#include "compiler.h"
#include "symbols.h"
#include <dirent.h>
#include <string.h>
#include <stdio.h>

static int compilerInited = 0;

int InitCompiler(void) {
    // 初始化符号表（类作用域 + 第一个子例程作用域）
    initSymbolTable();
    compilerInited = 1;
    return 1;
}

ParserInfo compile(const char* dir_name) {
    ParserInfo pi;
    pi.er = none;
    pi.tk.tp = EOFile;

    if (!compilerInited) {
        // 必须先调用 InitCompiler
        pi.er = syntaxError;
        strcpy(pi.tk.lx, "Compiler not initialized");
        return pi;
    }

    // 打开目录
    DIR* d = opendir(dir_name);
    if (!d) {
        pi.er = syntaxError;
        strcpy(pi.tk.lx, "Cannot open directory");
        return pi;
    }

    struct dirent* entry;
    // 逐个 .jack 文件处理
    while ((entry = readdir(d)) != NULL) {
        const char* name = entry->d_name;
        size_t L = strlen(name);
        if (L > 5 && strcmp(name + L - 5, ".jack") == 0) {
            // 构造完整路径
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", dir_name, name);

            // 每个类文件都要重新开始它的子例程作用域检查
            // （但保留类作用域）
            startSubroutine();

            // 初始化解析器
            if (!InitParser(path)) {
                pi.er = syntaxError;
                strcpy(pi.tk.lx, "Parser init failed");
                closedir(d);
                return pi;
            }

            // 语法 + 语义（符号表）检查
            pi = Parse();
            StopParser();
            if (pi.er != none) {
                closedir(d);
                return pi;
            }

            // TODO: 这里插入真正的代码生成调用，比如
            //    writeVM(path, ...);
            // 或者把每个 .jack 文件生成 .vm 文件
        }
    }
    closedir(d);
    return pi;
}

int StopCompiler(void) {
    // 目前无额外资源需要清理
    compilerInited = 0;
    return 1;
}

#ifndef TEST_COMPILER
int main(void) {
    char dir[256];
    printf("Enter directory containing .jack files:\n");
    scanf("%255s", dir);

    InitCompiler();
    ParserInfo res = compile(dir);
    if (res.er == none) {
        printf("Compilation successful!\n");
    } else {
        printf("Error type: %d, lexeme=\"%s\", line=%d\n",
               res.er, res.tk.lx, res.tk.ln);
    }
    StopCompiler();
    return 0;
}
#endif
