#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"

// ----------------------------
// 全局变量
// ----------------------------
static FILE *sourceFile = NULL;
static int currentChar;            // 当前读取的字符
static int currentLine = 1;        // 当前行号
static int peeked = 0;            // 是否已经预读
static Token peekToken;           // 预读到的 Token
static char globalFileName[32] = "";

// ----------------------------
// 关键字列表
// ----------------------------
static const char* keywords[] = {
    "class", "constructor", "function", "method", "field",
    "static", "var", "int", "char", "boolean",
    "void", "true", "false", "null", "this",
    "let", "do", "if", "else", "while", "return"
};
static const int numKeywords = 21;

// 允许的符号集合
static const char *allowedSymbols = "{}()[].,;+-*/&|<>=~";

// ----------------------------
// 辅助函数声明
// ----------------------------
static int readChar();
static Token skipWhitespaceAndComments();
static Token readIdentifier();
static Token readNumber();
static Token readString();
static Token readSymbol();
static void resetPeek();

// ----------------------------
// 初始化词法分析器
// ----------------------------
int InitLexer(char* file_name) {
    strncpy(globalFileName, file_name, sizeof(globalFileName)-1);
    sourceFile = fopen(file_name, "r");
    if (!sourceFile) {
        fprintf(stderr, "Error: Cannot open file %s\n", file_name);
        return 0;
    }
    currentLine = 1;
    currentChar = fgetc(sourceFile);
    peeked = 0;
    return 1;
}

// ----------------------------
// 停止词法分析器，释放资源
// ----------------------------
int StopLexer() {
    if (sourceFile) {
        fclose(sourceFile);
        sourceFile = NULL;
    }
    return 1;
}

// ----------------------------
// 读取下一个 Token
// ----------------------------
Token GetNextToken() {
    // 如果已经预读过 Token，则先返回预读的
    if (peeked) {
        peeked = 0;
        return peekToken;
    }

    // 跳过空白与注释，若检测到错误则直接返回
    Token skipToken = skipWhitespaceAndComments();
    if (skipToken.tp == ERR) {
        return skipToken;
    }

    // 若跳过后到达文件末尾
    if (currentChar == EOF) {
        Token t;
        t.tp = EOFile;
        strcpy(t.lx, "EOF");
        t.ec = 0;
        t.ln = currentLine;
        strncpy(t.fl, globalFileName, sizeof(t.fl)-1);
        return t;
    }

    // 根据字符类型调用不同的读取函数
    if (isalpha(currentChar) || currentChar == '_') {
        return readIdentifier();
    }
    else if (isdigit(currentChar)) {
        return readNumber();
    }
    else if (currentChar == '"') {
        return readString();
    }
    else {
        return readSymbol();
    }
}

// ----------------------------
// 预览下一个 Token（不消费）
// ----------------------------
Token PeekNextToken() {
    if (!peeked) {
        peekToken = GetNextToken();
        peeked = 1;
    }
    return peekToken;
}

// ----------------------------
// 辅助函数：读取下一个字符并更新行号
// ----------------------------
static int readChar() {
    int c = fgetc(sourceFile);
    if (c == '\n') {
        currentLine++;
    }
    return c;
}

// ----------------------------
// 辅助函数：跳过空白和注释
// ----------------------------
static Token skipWhitespaceAndComments() {
    Token errToken;
    errToken.tp = EOFile;  // 用 EOFile 表示“无错误且尚未真正到 EOF”
    errToken.lx[0] = '\0';
    errToken.ec = 0;
    errToken.ln = currentLine;
    strncpy(errToken.fl, globalFileName, sizeof(errToken.fl)-1);

    while (1) {
        // 1. 跳过空白字符
        while (isspace(currentChar)) {
            currentChar = readChar();
        }

        // 如果不是 '/', 表示当前不是注释开头
        if (currentChar != '/') {
            // 跳过空白后退出循环
            break;
        }

        // currentChar 为 '/', 需要看下一个字符
        int nextC = readChar();
        if (nextC == '/') {
            // 单行注释：跳过到行尾或 EOF
            while (currentChar != '\n' && currentChar != EOF) {
                currentChar = readChar();
            }
            // 读取下一个字符，准备继续循环
            currentChar = readChar();
        }
        else if (nextC == '*') {
            // 多行注释：一直读到 "*/" 或 EOF
            int foundEnd = 0;
            while (!foundEnd) {
                currentChar = readChar();
                if (currentChar == EOF) {
                    // 注释未闭合 => 返回错误
                    Token t;
                    t.tp = ERR;
                    strcpy(t.lx, "Error: unexpected eof in comment");
                    t.ec = EofInCom;
                    t.ln = currentLine;
                    strncpy(t.fl, globalFileName, sizeof(t.fl)-1);
                    return t;
                }
                else if (currentChar == '*') {
                    int maybeSlash = readChar();
                    if (maybeSlash == '/') {
                        // 找到 "*/"
                        currentChar = readChar(); // 跳到 */ 后下一个字符
                        foundEnd = 1;
                    }
                    else {
                        // 不是 '/', 退回
                        ungetc(maybeSlash, sourceFile);
                    }
                }
            }
        }
        else {
            // 既不是 // 也不是 /*，则把 nextC 退回，让后面当符号处理
            ungetc(nextC, sourceFile);
            break;
        }
    }

    return errToken;
}

// ----------------------------
// 辅助函数：读标识符或关键字
// ----------------------------
static Token readIdentifier() {
    Token t;
    t.ln = currentLine;
    strncpy(t.fl, globalFileName, sizeof(t.fl)-1);
    t.ec = 0;

    int idx = 0;
    while (isalnum(currentChar) || currentChar == '_') {
        if (idx < (int)sizeof(t.lx) - 1) {
            t.lx[idx++] = (char)currentChar;
        }
        currentChar = readChar();
    }
    t.lx[idx] = '\0';

    // 先默认设为 ID
    t.tp = ID;
    // 检查是否为关键字
    for (int i = 0; i < numKeywords; i++) {
        if (strcmp(t.lx, keywords[i]) == 0) {
            t.tp = RESWORD;
            break;
        }
    }
    return t;
}

// ----------------------------
// 辅助函数：读数字
// ----------------------------
static Token readNumber() {
    Token t;
    t.ln = currentLine;
    strncpy(t.fl, globalFileName, sizeof(t.fl)-1);
    t.ec = 0;
    
    int idx = 0;
    while (isdigit(currentChar)) {
        if (idx < (int)sizeof(t.lx) - 1) {
            t.lx[idx++] = (char)currentChar;
        }
        currentChar = readChar();
    }
    t.lx[idx] = '\0';
    t.tp = INT;
    return t;
}

// ----------------------------
// 辅助函数：读字符串
// ----------------------------
static Token readString() {
    Token t;
    // 把当前的行号存下来，以便后续报错时使用
    int startLine = currentLine;

    // 初始化 token
    t.tp = STRING;
    t.ln = startLine;
    strncpy(t.fl, globalFileName, sizeof(t.fl) - 1);
    t.ec = 0;

    // 跳过开头的双引号
    currentChar = readChar();

    int idx = 0;
    while (currentChar != '"' && currentChar != EOF) {
        if (currentChar == '\n') {
            // 如果遇到换行，报错时使用 startLine
            t.tp = ERR;
            strcpy(t.lx, "Error: new line in string constant");
            t.ec = NewLnInStr;
            t.ln = startLine;  // 这里使用字符串开始行，而不是 currentLine
            return t;
        }
        if (idx < (int)sizeof(t.lx) - 1) {
            t.lx[idx++] = (char)currentChar;
        }
        currentChar = readChar();
    }

    if (currentChar == EOF) {
        t.tp = ERR;
        strcpy(t.lx, "Error: unexpected eof in string constant");
        t.ec = EofInStr;
        t.ln = startLine;  // 同理，如果是EOF，也用字符串开始行号
        return t;
    }

    // 结束引号
    t.lx[idx] = '\0';
    // 跳过双引号
    currentChar = readChar();
    return t;
}


// ----------------------------
// 辅助函数：读符号或非法符号
// ----------------------------
static Token readSymbol() {
    Token t;
    t.ln = currentLine;
    strncpy(t.fl, globalFileName, sizeof(t.fl)-1);
    t.ec = 0;

    // 判断非法符号
    if (!strchr(allowedSymbols, currentChar)) {
        // 不在合法符号列表
        t.tp = ERR;
        strcpy(t.lx, "Error: illegal symbol in source file");
        t.ec = IllSym;
        // 消耗该字符
        currentChar = readChar();
    }
    else {
        // 合法符号
        t.tp = SYMBOL;
        t.lx[0] = (char)currentChar;
        t.lx[1] = '\0';
        currentChar = readChar();
    }
    return t;
}

// ----------------------------
// (可选) main函数用于你自己测试
// 提交时可注释或保留在 #ifndef TEST
// ----------------------------
#ifndef TEST
int main(void) {
    char filename[256];
    printf("请输入要测试的 JACK 源文件名：\n");
    scanf("%255s", filename);

    if (!InitLexer(filename)) {
        fprintf(stderr, "无法打开文件：%s\n", filename);
        return 1;
    }

    while (1) {
        Token tk = GetNextToken();
        printf("行号: %d, 内容: %s, 类型: %d\n", tk.ln, tk.lx, tk.tp);
        if (tk.tp == EOFile || tk.tp == ERR) {
            break;
        }
    }

    StopLexer();
    return 0;
}
#endif
