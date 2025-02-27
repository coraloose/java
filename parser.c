#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "lexer.h"
#include "parser.h"

//--------------------------------------------------
// 全局或静态变量
//--------------------------------------------------
static int parserInited = 0;

//--------------------------------------------------
// 内部函数声明（帮助解析不同语法结构）
//--------------------------------------------------
static ParserInfo parseClass();
static ParserInfo parseClassVarDec();
static ParserInfo parseSubroutineDec();
static ParserInfo parseParameterList();
static ParserInfo parseSubroutineBody();
static ParserInfo parseVarDec();
static ParserInfo parseStatements();
static ParserInfo parseStatement();
static ParserInfo parseLetStatement();
static ParserInfo parseIfStatement();
static ParserInfo parseWhileStatement();
static ParserInfo parseDoStatement();
static ParserInfo parseReturnStatement();
static ParserInfo parseExpression();
static ParserInfo parseTerm(); 
static ParserInfo eat(TokenType tp, const char* lx, SyntaxErrors err); 
static ParserInfo eatAny(TokenType tp, SyntaxErrors err);

// 一个小工具函数，用于在返回 ParserInfo 时，记录出错的 Token
static ParserInfo makeError(SyntaxErrors er, Token tk) {
    ParserInfo pi;
    pi.er = er;
    pi.tk = tk;
    return pi;
}

//--------------------------------------------------
// 初始化解析器：初始化词法分析器
//--------------------------------------------------
int InitParser(char* file_name)
{
    if (!InitLexer(file_name)) {
        return 0; // 打开文件失败
    }
    parserInited = 1;
    return 1;
}

//--------------------------------------------------
// 主解析入口：Parse()
// 1) 确认已初始化
// 2) 调用 parseClass() 解析顶层的类结构
// 3) 若无错，再读一个token检查余留
//--------------------------------------------------
ParserInfo Parse()
{
    ParserInfo pi;
    pi.er = none;
    pi.tk.tp = EOFile;

    if (!parserInited) {
        pi.er = syntaxError;
        strcpy(pi.tk.lx, "Parser not initialized");
        return pi;
    }

    // 解析 class
    pi = parseClass();
    if (pi.er != none) {
        return pi;
    }

    // 可选：再读取一个 token，看是否多余内容
    Token tk = GetNextToken();
    if (tk.tp == ERR) {
        pi.er = lexerErr;
        pi.tk = tk;
    }
    // 如果不是 EOFile，你可以选择忽略，也可报 syntaxError

    return pi;
}

//--------------------------------------------------
// 停止解析器
//--------------------------------------------------
int StopParser()
{
    StopLexer();
    parserInited = 0;
    return 1;
}

//--------------------------------------------------
// parseClass:
//  class <identifier> { classVarDec* subroutineDec* }
//  如果缺 } 就会抛出 closeBraceExpected 等
//--------------------------------------------------
static ParserInfo parseClass()
{
    // 1) 'class'
    ParserInfo pi = eat(RESWORD, "class", classExpected);
    if (pi.er != none) return pi;

    // 2) className => identifier
    pi = eatAny(ID, idExpected);
    if (pi.er != none) return pi;

    // 3) '{'
    pi = eat(SYMBOL, "{", openBraceExpected);
    if (pi.er != none) return pi;

    // 4) classVarDec*
    while (1) {
        Token look = PeekNextToken();
        if (look.tp == ERR) {
            return makeError(lexerErr, look);
        }
        // 如果遇到 '}' 或是子程序关键字，就 break
        if (look.tp == SYMBOL && strcmp(look.lx, "}") == 0) {
            break;
        }
        if (look.tp == RESWORD && 
           (!strcmp(look.lx, "static") || !strcmp(look.lx, "field"))) 
        {
            // 解析 classVarDec
            pi = parseClassVarDec();
            if (pi.er != none) return pi;
        } else {
            // 不是 classVarDec => 可能是 subroutineDec 或结束
            break;
        }
    }

    // 5) subroutineDec*
    while (1) {
        Token look = PeekNextToken();
        if (look.tp == ERR) {
            return makeError(lexerErr, look);
        }
        if (look.tp == SYMBOL && !strcmp(look.lx, "}")) {
            break;
        }
        if (look.tp == RESWORD && 
           (!strcmp(look.lx, "constructor") ||
            !strcmp(look.lx, "function") ||
            !strcmp(look.lx, "method"))) 
        {
            pi = parseSubroutineDec();
            if (pi.er != none) return pi;
        }
        else {
            // 不是 subroutineDec => break
            break;
        }
    }

    // 6) '}'
    pi = eat(SYMBOL, "}", closeBraceExpected);
    return pi;
}

//--------------------------------------------------
// parseClassVarDec:
//  (static|field) type varName (, varName)* ;
//--------------------------------------------------
static ParserInfo parseClassVarDec()
{
    // 已在外部 peek 到 static|field，这里直接 consume
    Token first = GetNextToken();
    if (first.tp == ERR) return makeError(lexerErr, first);

    // 1) type
    ParserInfo pi;
    pi.er = none; 
    pi.tk.tp = EOFile;

    // type可以是 int char boolean 或 identifier
    Token tk = GetNextToken();
    if (tk.tp == ERR) return makeError(lexerErr, tk);
    if (tk.tp == RESWORD) {
        if (strcmp(tk.lx,"int") && strcmp(tk.lx,"char") && strcmp(tk.lx,"boolean")) {
            return makeError(illegalType, tk);
        }
    } else if (tk.tp != ID) {
        return makeError(illegalType, tk);
    }

    // 2) varName
    Token varTk = GetNextToken();
    if (varTk.tp == ERR) return makeError(lexerErr, varTk);
    if (varTk.tp != ID) {
        return makeError(idExpected, varTk);
    }

    // 3) (, varName)*
    while (1) {
        Token look = PeekNextToken();
        if (look.tp == ERR) return makeError(lexerErr, look);
        if (look.tp == SYMBOL && !strcmp(look.lx, ",")) {
            GetNextToken(); // consume ','
            Token varN = GetNextToken();
            if (varN.tp != ID) {
                return makeError(idExpected, varN);
            }
        } else {
            break;
        }
    }

    // 4) ;
    pi = eat(SYMBOL, ";", semicolonExpected);
    return pi;
}

//--------------------------------------------------
// parseSubroutineDec:
//  (constructor|function|method) (void|type) subroutineName 
//  '(' parameterList ')' subroutineBody
//--------------------------------------------------
static ParserInfo parseSubroutineDec()
{
    // consume constructor|function|method
    Token first = GetNextToken();
    if (first.tp == ERR) return makeError(lexerErr, first);

    // return type: void|int|char|boolean|ID
    Token tkType = GetNextToken();
    if (tkType.tp == ERR) return makeError(lexerErr, tkType);
    if (tkType.tp == RESWORD) {
        if (strcmp(tkType.lx,"void") && strcmp(tkType.lx,"int")
            && strcmp(tkType.lx,"char") && strcmp(tkType.lx,"boolean")) 
        {
            return makeError(illegalType, tkType);
        }
    } else if (tkType.tp != ID) {
        return makeError(illegalType, tkType);
    }

    // subroutineName => identifier
    Token subName = GetNextToken();
    if (subName.tp == ERR) return makeError(lexerErr, subName);
    if (subName.tp != ID) {
        return makeError(idExpected, subName);
    }

    // '('
    ParserInfo pi = eat(SYMBOL, "(", openParenExpected);
    if (pi.er != none) return pi;

    // parameterList
    pi = parseParameterList();
    if (pi.er != none) return pi;

    // ')'
    pi = eat(SYMBOL, ")", closeParenExpected);
    if (pi.er != none) return pi;

    // subroutineBody => '{' varDec* statements '}'
    pi = parseSubroutineBody();
    return pi;
}

//--------------------------------------------------
// parseParameterList:
//  ( (type varName) (',' type varName)* )?
//  这里简单实现，不识别的话就直接返回
//--------------------------------------------------
static ParserInfo parseParameterList()
{
    ParserInfo pi;
    pi.er = none;
    pi.tk.tp = EOFile;

    // 看看下一个 token 是否为 ')', 如果是则空参数
    Token look = PeekNextToken();
    if (look.tp == SYMBOL && !strcmp(look.lx,")")) {
        return pi; // 空参数列表
    }
    // 否则读取至少一个 "type varName"
    // type
    Token tkType = GetNextToken();
    if (tkType.tp == ERR) return makeError(lexerErr, tkType);

    if (tkType.tp == RESWORD) {
        if (strcmp(tkType.lx,"int") && strcmp(tkType.lx,"char") && strcmp(tkType.lx,"boolean")) {
            return makeError(illegalType, tkType);
        }
    } else if (tkType.tp != ID) {
        return makeError(illegalType, tkType);
    }
    // varName
    Token varN = GetNextToken();
    if (varN.tp != ID) {
        return makeError(idExpected, varN);
    }

    // (, type varName)* 
    while (1) {
        Token look2 = PeekNextToken();
        if (look2.tp == SYMBOL && !strcmp(look2.lx,",")) {
            GetNextToken(); // consume ','
            // type
            Token nxtType = GetNextToken();
            if (nxtType.tp == ERR) return makeError(lexerErr, nxtType);
            if (nxtType.tp == RESWORD) {
                if (strcmp(nxtType.lx,"int") && strcmp(nxtType.lx,"char") && strcmp(nxtType.lx,"boolean")) {
                    return makeError(illegalType, nxtType);
                }
            } else if (nxtType.tp != ID) {
                return makeError(illegalType, nxtType);
            }
            // varName
            Token nxtVar = GetNextToken();
            if (nxtVar.tp != ID) {
                return makeError(idExpected, nxtVar);
            }
        } else {
            break;
        }
    }

    return pi;
}

//--------------------------------------------------
// parseSubroutineBody:
//  '{' varDec* statements '}'
//--------------------------------------------------
static ParserInfo parseSubroutineBody()
{
    ParserInfo pi = eat(SYMBOL, "{", openBraceExpected);
    if (pi.er != none) return pi;

    // varDec*
    while (1) {
        Token look = PeekNextToken();
        if (look.tp == ERR) {
            return makeError(lexerErr, look);
        }
        if (look.tp == RESWORD && !strcmp(look.lx, "var")) {
            pi = parseVarDec();
            if (pi.er != none) return pi;
        } else {
            break;
        }
    }

    // statements
    pi = parseStatements();
    if (pi.er != none) return pi;

    // '}'
    pi = eat(SYMBOL, "}", closeBraceExpected);
    return pi;
}

//--------------------------------------------------
// parseVarDec: var type varName (, varName)* ;
//--------------------------------------------------
static ParserInfo parseVarDec()
{
    // consume 'var'
    Token first = GetNextToken();
    if (first.tp == ERR) return makeError(lexerErr, first);

    // type
    Token tkType = GetNextToken();
    if (tkType.tp == ERR) return makeError(lexerErr, tkType);
    if (tkType.tp == RESWORD) {
        if (strcmp(tkType.lx,"int") && strcmp(tkType.lx,"char") && strcmp(tkType.lx,"boolean")) {
            return makeError(illegalType, tkType);
        }
    } else if (tkType.tp != ID) {
        return makeError(illegalType, tkType);
    }

    // varName
    Token varN = GetNextToken();
    if (varN.tp != ID) {
        return makeError(idExpected, varN);
    }

    // (, varName)*
    while (1) {
        Token look = PeekNextToken();
        if (look.tp == SYMBOL && !strcmp(look.lx, ",")) {
            GetNextToken(); // consume ','
            Token nxt = GetNextToken();
            if (nxt.tp != ID) {
                return makeError(idExpected, nxt);
            }
        } else {
            break;
        }
    }

    // ';'
    ParserInfo pi = eat(SYMBOL, ";", semicolonExpected);
    return pi;
}

//--------------------------------------------------
// parseStatements: statement*
//  statement => let | if | while | do | return
//--------------------------------------------------
static ParserInfo parseStatements()
{
    ParserInfo pi;
    pi.er = none;
    pi.tk.tp = EOFile;

    while (1) {
        Token look = PeekNextToken();
        if (look.tp == ERR) {
            return makeError(lexerErr, look);
        }
        // 如果遇到 '}', 说明结束了
        if (look.tp == SYMBOL && !strcmp(look.lx, "}")) {
            break; // statements结束
        }
        // 否则根据关键字分发
        if (look.tp == RESWORD && !strcmp(look.lx,"let")) {
            pi = parseLetStatement();
        } else if (look.tp == RESWORD && !strcmp(look.lx,"if")) {
            pi = parseIfStatement();
        } else if (look.tp == RESWORD && !strcmp(look.lx,"while")) {
            pi = parseWhileStatement();
        } else if (look.tp == RESWORD && !strcmp(look.lx,"do")) {
            pi = parseDoStatement();
        } else if (look.tp == RESWORD && !strcmp(look.lx,"return")) {
            pi = parseReturnStatement();
        } else {
            // 出现了意料之外的语句 => syntaxError
            return makeError(syntaxError, look);
        }
        if (pi.er != none) return pi;
    }
    return pi;
}

//--------------------------------------------------
// parseStatement: let / if / while / do / return
//--------------------------------------------------
static ParserInfo parseLetStatement()
{
    // consume 'let'
    Token letTk = GetNextToken();
    if (letTk.tp == ERR) return makeError(lexerErr, letTk);

    // varName
    Token varN = GetNextToken();
    if (varN.tp != ID) {
        return makeError(idExpected, varN);
    }

    // check optional [ expression ]
    Token look = PeekNextToken();
    if (look.tp == SYMBOL && !strcmp(look.lx,"[")) {
        // consume '['
        GetNextToken();
        ParserInfo pi = parseExpression();
        if (pi.er != none) return pi;

        // expect ']'
        pi = eat(SYMBOL, "]", closeBracketExpected);
        if (pi.er != none) return pi;
    }

    // '='
    ParserInfo pi = eat(SYMBOL, "=", equalExpected);
    if (pi.er != none) return pi;

    // expression
    pi = parseExpression();
    if (pi.er != none) return pi;

    // ';'
    pi = eat(SYMBOL, ";", semicolonExpected);
    return pi;
}

static ParserInfo parseIfStatement()
{
    // consume 'if'
    Token ifTk = GetNextToken();
    if (ifTk.tp == ERR) return makeError(lexerErr, ifTk);

    // '('
    ParserInfo pi = eat(SYMBOL, "(", openParenExpected);
    if (pi.er != none) return pi;

    // expression
    pi = parseExpression();
    if (pi.er != none) return pi;

    // ')'
    pi = eat(SYMBOL, ")", closeParenExpected);
    if (pi.er != none) return pi;

    // '{' statements '}'
    pi = eat(SYMBOL, "{", openBraceExpected);
    if (pi.er != none) return pi;
    pi = parseStatements();
    if (pi.er != none) return pi;
    pi = eat(SYMBOL, "}", closeBraceExpected);
    if (pi.er != none) return pi;

    // optional else
    Token look = PeekNextToken();
    if (look.tp == RESWORD && !strcmp(look.lx,"else")) {
        GetNextToken(); // consume else
        // '{' statements '}'
        pi = eat(SYMBOL, "{", openBraceExpected);
        if (pi.er != none) return pi;
        pi = parseStatements();
        if (pi.er != none) return pi;
        pi = eat(SYMBOL, "}", closeBraceExpected);
        if (pi.er != none) return pi;
    }

    return pi;
}

static ParserInfo parseWhileStatement()
{
    // consume 'while'
    Token wtk = GetNextToken();
    if (wtk.tp == ERR) return makeError(lexerErr, wtk);

    // '(' expression ')'
    ParserInfo pi = eat(SYMBOL, "(", openParenExpected);
    if (pi.er != none) return pi;

    pi = parseExpression();
    if (pi.er != none) return pi;

    pi = eat(SYMBOL, ")", closeParenExpected);
    if (pi.er != none) return pi;

    // '{' statements '}'
    pi = eat(SYMBOL, "{", openBraceExpected);
    if (pi.er != none) return pi;

    pi = parseStatements();
    if (pi.er != none) return pi;

    pi = eat(SYMBOL, "}", closeBraceExpected);
    return pi;
}

static ParserInfo parseDoStatement()
{
    // consume 'do'
    Token dtk = GetNextToken();
    if (dtk.tp == ERR) return makeError(lexerErr, dtk);

    // subroutineCall => 
    //   (className|varName) '.' subroutineName '(' exprList ')' 
    //   or just subroutineName '(' exprList ')'
    // 简化：只要 parseExpression() 里尽量跳到 ';'
    // 但是这里最好做最小校验
    Token first = GetNextToken();
    if (first.tp != ID) {
        return makeError(idExpected, first);
    }
    // check '.' or '('
    Token look = PeekNextToken();
    if (look.tp == SYMBOL && !strcmp(look.lx,".")) {
        GetNextToken(); // consume '.'
        Token subN = GetNextToken();
        if (subN.tp != ID) {
            return makeError(idExpected, subN);
        }
    }
    // '('
    ParserInfo pi = eat(SYMBOL, "(", openParenExpected);
    if (pi.er != none) return pi;
    // parse expressionList (这里直接用 parseExpression 省略，或者不 parse)
    // 简化：多次解析 expression, 用逗号分隔 
    look = PeekNextToken();
    if (look.tp == ERR) return makeError(lexerErr, look);
    if (!(look.tp == SYMBOL && !strcmp(look.lx,")"))) {
        // 说明不是空列表
        pi = parseExpression();
        if (pi.er != none) return pi;
        while (1) {
            Token look2 = PeekNextToken();
            if (look2.tp == SYMBOL && !strcmp(look2.lx,",")) {
                GetNextToken(); // consume ','
                pi = parseExpression();
                if (pi.er != none) return pi;
            } else {
                break;
            }
        }
    }
    // ')'
    pi = eat(SYMBOL, ")", closeParenExpected);
    if (pi.er != none) return pi;

    // ';'
    pi = eat(SYMBOL, ";", semicolonExpected);
    return pi;
}

static ParserInfo parseReturnStatement()
{
    // consume 'return'
    Token rtk = GetNextToken();
    if (rtk.tp == ERR) return makeError(lexerErr, rtk);

    // optional expression
    Token look = PeekNextToken();
    if (look.tp == ERR) return makeError(lexerErr, look);
    if (!(look.tp == SYMBOL && !strcmp(look.lx, ";"))) {
        // 不是分号 => 说明有表达式
        ParserInfo pi = parseExpression();
        if (pi.er != none) return pi;
    }

    // ';'
    return eat(SYMBOL, ";", semicolonExpected);
}

//--------------------------------------------------
// parseExpression: 这里仅做最小解析
//  expression => term (op term)* 
//  op => + - * / & | < > = 
//--------------------------------------------------
static ParserInfo parseExpression()
{
    ParserInfo pi = parseTerm();
    if (pi.er != none) return pi;

    while (1) {
        Token look = PeekNextToken();
        if (look.tp == ERR) return makeError(lexerErr, look);

        if (look.tp == SYMBOL && 
           (!strcmp(look.lx,"+") || !strcmp(look.lx,"-") ||
            !strcmp(look.lx,"*") || !strcmp(look.lx,"/") ||
            !strcmp(look.lx,"&") || !strcmp(look.lx,"|") ||
            !strcmp(look.lx,"<") || !strcmp(look.lx,">") ||
            !strcmp(look.lx,"="))) 
        {
            // consume op
            GetNextToken();
            pi = parseTerm();
            if (pi.er != none) return pi;
        } else {
            break;
        }
    }
    return pi;
}

//--------------------------------------------------
// parseTerm:
// term => intConst | stringConst | keywordConst | varName([...])?
//        | subroutineCall
//        | ( expr ) 
//        | unaryOp term
//  这里只做极简实现
//--------------------------------------------------
static ParserInfo parseTerm()
{
    Token tk = GetNextToken();
    if (tk.tp == ERR) {
        return makeError(lexerErr, tk);
    }
    // 根据 tk 类型判定
    if (tk.tp == INT || tk.tp == STRING) {
        // intConst 或 stringConst => 直接返回
        ParserInfo pi;
        pi.er = none;
        pi.tk = tk;
        return pi;
    }
    else if (tk.tp == RESWORD) {
        // true, false, null, this
        // 最简都接受
        if (!strcmp(tk.lx,"true") || !strcmp(tk.lx,"false") ||
            !strcmp(tk.lx,"null") || !strcmp(tk.lx,"this")) 
        {
            ParserInfo pi;
            pi.er = none;
            pi.tk = tk;
            return pi;
        }
        // 否则认为是syntaxError
        return makeError(syntaxError, tk);
    }
    else if (tk.tp == SYMBOL) {
        // '(' expr ')' 或 unaryOp term
        if (!strcmp(tk.lx,"(")) {
            // parseExpression
            ParserInfo pi = parseExpression();
            if (pi.er != none) return pi;
            // expect ')'
            pi = eat(SYMBOL, ")", closeParenExpected);
            return pi;
        }
        // unaryOp => - or ~
        else if (!strcmp(tk.lx,"-") || !strcmp(tk.lx,"~")) {
            // parseTerm again
            return parseTerm();
        }
        else {
            return makeError(syntaxError, tk);
        }
    }
    else if (tk.tp == ID) {
        // varName 或 subroutineCall
        // 需要看下一个 token
        Token look = PeekNextToken();
        if (look.tp == SYMBOL && !strcmp(look.lx,"[")) {
            // varName '[' expression ']'
            GetNextToken(); // consume '['
            ParserInfo pi = parseExpression();
            if (pi.er != none) return pi;
            pi = eat(SYMBOL, "]", closeBracketExpected);
            return pi;
        }
        else if (look.tp == SYMBOL && (!strcmp(look.lx,"(") || !strcmp(look.lx,"."))) {
            // subroutineCall
            // 这里可复用 parseDoStatement 的那套逻辑，但简化
            // 1) maybe '.' ID
            if (!strcmp(look.lx,".")) {
                GetNextToken(); // consume '.'
                Token subN = GetNextToken();
                if (subN.tp != ID) {
                    return makeError(idExpected, subN);
                }
            }
            // '('
            ParserInfo pi = eat(SYMBOL, "(", openParenExpected);
            if (pi.er != none) return pi;
            // parse expressionList (简化)
            Token look2 = PeekNextToken();
            if (look2.tp == ERR) return makeError(lexerErr, look2);
            if (!(look2.tp == SYMBOL && !strcmp(look2.lx,")"))) {
                // at least one expr
                pi = parseExpression();
                if (pi.er != none) return pi;
                while (1) {
                    Token look3 = PeekNextToken();
                    if (look3.tp == SYMBOL && !strcmp(look3.lx,",")) {
                        GetNextToken(); 
                        pi = parseExpression();
                        if (pi.er != none) return pi;
                    } else {
                        break;
                    }
                }
            }
            pi = eat(SYMBOL, ")", closeParenExpected);
            return pi;
        }
        else {
            // just varName
            ParserInfo pi;
            pi.er = none;
            pi.tk = tk;
            return pi;
        }
    }
    else {
        return makeError(syntaxError, tk);
    }
}

//--------------------------------------------------
// eat: 期望下一个 token 为 (tp,lx)，若不匹配则报对应的 err
//--------------------------------------------------
static ParserInfo eat(TokenType tp, const char* lx, SyntaxErrors err)
{
    Token tk = GetNextToken();
    if (tk.tp == ERR) {
        // lexer error
        ParserInfo pi;
        pi.er = lexerErr;
        pi.tk = tk;
        return pi;
    }
    if (tk.tp != tp || (lx && strcmp(tk.lx,lx))) {
        // 不匹配
        return makeError(err, tk);
    }
    // 匹配成功
    ParserInfo pi;
    pi.er = none;
    pi.tk = tk;
    return pi;
}

//--------------------------------------------------
// eatAny: 期望下一个 token 为 (tp, 任意lexeme)，
// 用于idExpected等简单情况
//--------------------------------------------------
static ParserInfo eatAny(TokenType tp, SyntaxErrors err)
{
    Token tk = GetNextToken();
    if (tk.tp == ERR) {
        ParserInfo pi;
        pi.er = lexerErr;
        pi.tk = tk;
        return pi;
    }
    if (tk.tp != tp) {
        return makeError(err, tk);
    }
    ParserInfo pi;
    pi.er = none;
    pi.tk = tk;
    return pi;
}

//--------------------------------------------------
// main函数仅用于自测
//--------------------------------------------------
#ifndef TEST_PARSER
int main()
{
    char filename[256];
    printf("Enter JACK file name to parse:\n");
    scanf("%255s", filename);

    if (!InitParser(filename)) {
        fprintf(stderr, "Failed to init parser.\n");
        return 1;
    }

    ParserInfo result = Parse();
    if (result.er == none) {
        printf("Parse success, no syntax errors.\n");
    } else {
        printf("Parse error type: ");
        switch(result.er) {
            case none: printf("none"); break;
            case lexerErr: printf("lexer error"); break;
            case classExpected: printf("class expected"); break;
            case idExpected: printf("identifier expected"); break;
            case openBraceExpected: printf("{ expected"); break;
            case closeBraceExpected: printf("} expected"); break;
            case memberDeclarErr: printf("member declaration error"); break;
            case classVarErr: printf("class var error"); break;
            case illegalType: printf("illegal type"); break;
            case semicolonExpected: printf("; expected"); break;
            case subroutineDeclarErr: printf("subroutine dec error"); break;
            case openParenExpected: printf("( expected"); break;
            case closeParenExpected: printf(") expected"); break;
            case closeBracketExpected: printf("] expected"); break;
            case equalExpected: printf("= expected"); break;
            case syntaxError: printf("syntax error"); break;
        }
        printf(", line: %d, token: %s\n", result.tk.ln, result.tk.lx);
    }

    StopParser();
    return 0;
}
#endif
