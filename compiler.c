// compiler.c
/************************************************************************
 University of Leeds
 School of Computing
 COMP2932- Compiler Design and Construction
 Compiler Module
*************************************************************************/
#include "compiler.h"
#include "symbols.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>

static int compilerInited = 0;

int InitCompiler(void) {
    initSymbolTable();   // 清空/初始化符号表
    compilerInited = 1;
    return 1;
}

ParserInfo compile(const char* dir_name) {
    ParserInfo pi;
    pi.er = none;

    if (!compilerInited) {
        pi.er = syntaxError;
        strcpy(pi.tk.lx, "Compiler not initialized");
        return pi;
    }

    DIR* d = opendir(dir_name);
    if (!d) {
        pi.er = syntaxError;
        strcpy(pi.tk.lx, "Cannot open directory");
        return pi;
    }

    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        size_t L = strlen(e->d_name);
        if (L > 5 && strcmp(e->d_name + L - 5, ".jack") == 0) {
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", dir_name, e->d_name);

            // 每个 .jack 文件开始前，重置子例程作用域
            startSubroutine();

            if (!InitParser(path)) {
                pi.er = syntaxError;
                strcpy(pi.tk.lx, "Parser init failed");
                closedir(d);
                return pi;
            }

            pi = Parse();
            StopParser();
            if (pi.er != none) {
                closedir(d);
                return pi;
            }

            // TODO: 这里可以调用你的代码生成模块，写出 .vm 文件
        }
    }
    closedir(d);
    return pi;
}

int StopCompiler(void) {
    compilerInited = 0;
    return 1;
}

#ifdef TEST_COMPILER
int main(void) {
    char dir[256];
    printf("Enter directory with .jack files:\n");
    scanf("%255s", dir);

    InitCompiler();
    ParserInfo res = compile(dir);
    if (res.er == none) {
        printf("Compilation successful!\n");
    } else {
        printf("Error type: %d lexeme=\"%s\" line=%d\n",
               res.er, res.tk.lx, res.tk.ln);
    }
    StopCompiler();
    return 0;
}
#endif
