


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"


// YOU CAN ADD YOUR OWN FUNCTIONS, DECLARATIONS AND VARIABLES HERE
static FILE *sourceFile = NULL;
static int currentChar;
static int currentLine = 1;
static int peeked = 0;
static Token peekToken;
static char globalFileName[32] = "";  // 用于存储文件名，填充 Token.fl 时使用

// IMPLEMENT THE FOLLOWING functions
//***********************************

// Initialise the lexer to read from source file
// file_name is the name of the source file
// This requires opening the file and making any necessary initialisations of the lexer
// If an error occurs, the function should return 0
// if everything goes well the function should return 1
int InitLexer(char* file_name) {
  strncpy(globalFileName, file_name, sizeof(globalFileName)-1);
  sourceFile = fopen(file_name, "r");
  if (sourceFile == NULL) {
      fprintf(stderr, "Error: Cannot open file %s\n", file_name);
      return 0;
  }
  currentLine = 1;
  currentChar = fgetc(sourceFile);
  peeked = 0;
  return 1;
}

static int readChar() {
  int c = fgetc(sourceFile);
  if (c == '\n') {
      currentLine++;
  }
  return c;
}

static void skipWhitespace() {
  while (isspace(currentChar)) {
      currentChar = readChar();
  }
}

static Token readIdentifier() {
  Token t;
  t.ln = currentLine;
  strncpy(t.fl, globalFileName, sizeof(t.fl)-1);
  int idx = 0;
  while (isalnum(currentChar) || currentChar == '_') {
      if (idx < sizeof(t.lx)-1)
          t.lx[idx++] = currentChar;
      currentChar = readChar();
  }
  t.lx[idx] = '\0';
  // 判断是否是关键字，示例仅将其设为标识符
  t.tp = ID;  
  // 你可以比较 t.lx 与关键字列表，将 t.tp 设为 RESWORD 时机
  return t;
}

static Token readNumber() {
  Token t;
  t.ln = currentLine;
  strncpy(t.fl, globalFileName, sizeof(t.fl)-1);
  int idx = 0;
  while (isdigit(currentChar)) {
      if (idx < sizeof(t.lx)-1)
          t.lx[idx++] = currentChar;
      currentChar = readChar();
  }
  t.lx[idx] = '\0';
  t.tp = INT;
  return t;
}

static Token readString() {
  Token t;
  t.ln = currentLine;
  strncpy(t.fl, globalFileName, sizeof(t.fl)-1);
  int idx = 0;
  // 跳过开头的双引号
  currentChar = readChar();
  while (currentChar != '"' && currentChar != EOF) {
      if (currentChar == '\n') {
          t.tp = ERR;
          strcpy(t.lx, "NewLnInStr");
          t.ec = NewLnInStr;
          return t;
      }
      if (idx < sizeof(t.lx)-1)
          t.lx[idx++] = currentChar;
      currentChar = readChar();
  }
  if (currentChar == EOF) {
      t.tp = ERR;
      strcpy(t.lx, "EofInStr");
      t.ec = EofInStr;
      return t;
  }
  t.lx[idx] = '\0';
  t.tp = STRING;
  // 跳过结束的双引号
  currentChar = readChar();
  return t;
}

static Token readSymbol() {
  Token t;
  t.ln = currentLine;
  strncpy(t.fl, globalFileName, sizeof(t.fl)-1);
  t.lx[0] = currentChar;
  t.lx[1] = '\0';
  t.tp = SYMBOL;
  currentChar = readChar();
  return t;
}

// Get the next token from the source file
Token GetNextToken() {
  Token t;
  if (peeked) {
      peeked = 0;
      return peekToken;
  }
  skipWhitespace();
  // 这里可以调用 skipComments(); 如果实现了注释处理

  t.ln = currentLine;
  strncpy(t.fl, globalFileName, sizeof(t.fl)-1);
  
  if (currentChar == EOF) {
      t.tp = EOFile;
      strcpy(t.lx, "EOF");
      return t;
  }
  
  if (isalpha(currentChar) || currentChar == '_')
      return readIdentifier();
  if (isdigit(currentChar))
      return readNumber();
  if (currentChar == '"')
      return readString();
  
  // 对其他字符，视为符号
  return readSymbol();
}

// peek (look) at the next token in the source file without removing it from the stream
Token PeekNextToken() {
  if (!peeked) {
      peekToken = GetNextToken();
      peeked = 1;
  }
  return peekToken;
}


// clean out at end, e.g. close files, free memory, ... etc
int StopLexer() {
  if (sourceFile) {
      fclose(sourceFile);
      sourceFile = NULL;
  }
  return 1;
}
// do not remove the next line
#ifndef TEST
int main(void) {
    char filename[256];
    printf("请输入要测试的JACK源文件名：\n");
    scanf("%255s", filename);

    if (!InitLexer(filename)) {
        fprintf(stderr, "无法打开文件：%s\n", filename);
        return 1;
    }

    Token token;
    // 循环读取记号，直到遇到文件结束或发生错误
    while (1) {
        token = GetNextToken();
        // 输出记号信息：行号、记号内容和记号类型
        printf("行号：%d, 内容：%s, 类型：%d\n", token.ln, token.lx, token.tp);
        if (token.tp == EOFile || token.tp == ERR) {
            break;
        }
    }

    StopLexer();
    return 0;
}
#endif
