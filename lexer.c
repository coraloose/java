



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

static Token skipWhitespaceAndComments() {
  Token errToken;
  errToken.tp = EOFile; // 用EOFile当做“无错误且未真正到文件末尾”的占位标记
  errToken.lx[0] = '\0';

  while (1) {
      // 先跳过空白字符
      while (isspace(currentChar)) {
          currentChar = readChar();
      }

      // 如果当前字符不是 '/'，就说明不是注释开头，退出循环
      if (currentChar != '/') {
          break;
      }

      // 如果是 '/'，需要看下一个字符以判断注释类型
      int nextC = fgetc(sourceFile);
      if (nextC == '/') {
          // 单行注释：跳过直到行尾或EOF
          while (currentChar != '\n' && currentChar != EOF) {
              currentChar = readChar();
          }
      } else if (nextC == '*') {
          // 多行注释：一直读到 "*/" 或EOF
          int foundEnd = 0;
          while (!foundEnd) {
              currentChar = readChar();
              if (currentChar == EOF) {
                  // 没等到 "*/" 就EOF => 错误记号
                  errToken.tp = ERR;
                  strcpy(errToken.lx, "Error: unexpected eof in comment");
                  errToken.ec = EofInCom; // 对应 EofInCom
                  errToken.ln = currentLine;
                  strncpy(errToken.fl, globalFileName, sizeof(errToken.fl) - 1);
                  return errToken;
              } 
              else if (currentChar == '\n') {
                  currentLine++;
              } 
              else if (currentChar == '*') {
                  // 看看下一个字符是不是 '/'
                  int maybeSlash = fgetc(sourceFile);
                  if (maybeSlash == '/') {
                      // 找到 "*/"
                      foundEnd = 1;
                      // 读取下一个字符，准备下次循环
                      currentChar = readChar();
                  } else {
                      // 不是 '/', 退回一个字符继续处理
                      ungetc(maybeSlash, sourceFile);
                  }
              }
          }
      } else {
          // 不是注释，可能只是一个 '/' 符号
          // 把 nextC 放回缓冲区，这样后面可以当符号处理
          ungetc(nextC, sourceFile);
          break;
      }
  }

  return errToken;
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

    // 先调用 skipWhitespaceAndComments
    Token skipToken = skipWhitespaceAndComments();
    if (skipToken.tp == ERR) {
        // 如果检测到多行注释未闭合，直接返回这个错误记号
        return skipToken;
    }

    // 如果 currentChar == EOF，说明没有更多可读内容
    if (currentChar == EOF) {
        Token t;
        t.tp = EOFile;
        strcpy(t.lx, "EOF");
        t.ec = 0;
        t.ln = currentLine;
        strncpy(t.fl, globalFileName, sizeof(t.fl) - 1);
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
