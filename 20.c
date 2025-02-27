#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "lexer.h"
#include "parser.h"

//---------------------------------------
// Global parser state
//---------------------------------------
static int parserInited = 0;

//---------------------------------------
// Forward declarations
//---------------------------------------
static ParserInfo parseClass();
static ParserInfo parseClassVarDec();
static ParserInfo parseSubroutineDec();
static ParserInfo parseParameterList();
static ParserInfo parseSubroutineBody();
static ParserInfo parseVarDec();
static ParserInfo parseStatements();
static ParserInfo parseLetStatement();
static ParserInfo parseIfStatement();
static ParserInfo parseWhileStatement();
static ParserInfo parseDoStatement();
static ParserInfo parseReturnStatement();
static ParserInfo parseExpression();
static ParserInfo parseTerm();

// Utility functions
static ParserInfo eat(TokenType tp, const char* lx, SyntaxErrors err);
static ParserInfo eatAny(TokenType tp, SyntaxErrors err);
static ParserInfo makeError(SyntaxErrors er, Token tk);

//---------------------------------------
// InitParser / Parse / StopParser
//---------------------------------------
int InitParser(char* file_name)
{
    if (!InitLexer(file_name)) {
        return 0;
    }
    parserInited = 1;
    return 1;
}

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

    pi = parseClass();
    if (pi.er != none) {
        return pi;
    }

    // Optionally check for leftover tokens (ignored here)
    Token tk = GetNextToken();
    if (tk.tp == ERR) {
        pi.er = lexerErr;
        pi.tk = tk;
    }
    return pi;
}

int StopParser()
{
    StopLexer();
    parserInited = 0;
    return 1;
}

//---------------------------------------
// parseClass: class <id> { classVarDec* subroutineDec* }
//---------------------------------------
static ParserInfo parseClass()
{
    ParserInfo pi = eat(RESWORD, "class", classExpected);
    if (pi.er != none) return pi;

    pi = eatAny(ID, idExpected);
    if (pi.er != none) return pi;

    pi = eat(SYMBOL, "{", openBraceExpected);
    if (pi.er != none) return pi;

    // Parse classVarDec*
    while (1) {
        Token look = PeekNextToken();
        if (look.tp == ERR) return makeError(lexerErr, look);
        if (look.tp == SYMBOL && strcmp(look.lx, "}") == 0) break;
        if (look.tp == RESWORD &&
           (!strcmp(look.lx, "static") || !strcmp(look.lx, "field"))) {
            pi = parseClassVarDec();
            if (pi.er != none) return pi;
        } else {
            break;
        }
    }

    // Parse subroutineDec*
    while (1) {
        Token look = PeekNextToken();
        if (look.tp == ERR) return makeError(lexerErr, look);
        if (look.tp == SYMBOL && strcmp(look.lx, "}") == 0) break;
        if (look.tp == RESWORD &&
           (!strcmp(look.lx, "constructor") ||
            !strcmp(look.lx, "function") ||
            !strcmp(look.lx, "method"))) {
            ParserInfo sp = parseSubroutineDec();
            if (sp.er != none) return sp;
        } else {
            break;
        }
    }

    pi = eat(SYMBOL, "}", closeBraceExpected);
    return pi;
}

//---------------------------------------
// parseClassVarDec: (static|field) type varName (, varName)* ;
//---------------------------------------
static ParserInfo parseClassVarDec()
{
    Token first = GetNextToken();
    if (first.tp == ERR) return makeError(lexerErr, first);

    // type
    Token tkType = GetNextToken();
    if (tkType.tp == ERR) return makeError(lexerErr, tkType);
    if (tkType.tp == RESWORD) {
        if (strcmp(tkType.lx, "int") && strcmp(tkType.lx, "char") && strcmp(tkType.lx, "boolean")) {
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

    // Handle additional var names
    while (1) {
        Token look = PeekNextToken();
        if (look.tp == SYMBOL && strcmp(look.lx, ",") == 0) {
            GetNextToken(); // consume ','
            Token nxt = GetNextToken();
            if (nxt.tp != ID) {
                return makeError(idExpected, nxt);
            }
        } else {
            break;
        }
    }

    ParserInfo pi = eat(SYMBOL, ";", semicolonExpected);
    return pi;
}

//---------------------------------------
// parseSubroutineDec: (constructor|function|method) (void|type) subroutineName ( parameterList ) subroutineBody
//---------------------------------------
static ParserInfo parseSubroutineDec()
{
    Token first = GetNextToken();
    if (first.tp == ERR) return makeError(lexerErr, first);

    // return type
    Token tkType = GetNextToken();
    if (tkType.tp == ERR) return makeError(lexerErr, tkType);
    if (tkType.tp == RESWORD) {
        if (strcmp(tkType.lx, "void") && strcmp(tkType.lx, "int") && strcmp(tkType.lx, "char") && strcmp(tkType.lx, "boolean")) {
            return makeError(illegalType, tkType);
        }
    } else if (tkType.tp != ID) {
        return makeError(illegalType, tkType);
    }

    // subroutine name
    Token subN = GetNextToken();
    if (subN.tp != ID) {
        return makeError(idExpected, subN);
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

    // subroutine body
    pi = parseSubroutineBody();
    return pi;
}

//---------------------------------------
// parseParameterList: ( (type varName) (',' type varName)* )?
// 若遇到不匹配 => return closeParenExpected error
//---------------------------------------
static ParserInfo parseParameterList()
{
    ParserInfo pi;
    pi.er = none;
    pi.tk.tp = EOFile;

    Token look = PeekNextToken();
    if (look.tp == SYMBOL && strcmp(look.lx, ")") == 0) {
        return pi; // empty list
    }

    // Read type
    Token tkType = GetNextToken();
    if (tkType.tp == ERR) return makeError(lexerErr, tkType);
    if (tkType.tp == SYMBOL) {
        // If we get a symbol (and it's not ")"), it's an error: we expected a type, so ) expected.
        return makeError(closeParenExpected, tkType);
    }
    if (tkType.tp == RESWORD) {
        if (strcmp(tkType.lx, "int") && strcmp(tkType.lx, "char") && strcmp(tkType.lx, "boolean")) {
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

    while (1) {
        Token look2 = PeekNextToken();
        if (look2.tp == SYMBOL && strcmp(look2.lx, ",") == 0) {
            GetNextToken(); // consume ','
            Token nxtType = GetNextToken();
            if (nxtType.tp == ERR) return makeError(lexerErr, nxtType);
            if (nxtType.tp == SYMBOL) {
                return makeError(closeParenExpected, nxtType);
            }
            if (nxtType.tp == RESWORD) {
                if (strcmp(nxtType.lx, "int") && strcmp(nxtType.lx, "char") && strcmp(nxtType.lx, "boolean")) {
                    return makeError(illegalType, nxtType);
                }
            } else if (nxtType.tp != ID) {
                return makeError(illegalType, nxtType);
            }
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

//---------------------------------------
// parseSubroutineBody: { varDec* statements }
//---------------------------------------
static ParserInfo parseSubroutineBody()
{
    ParserInfo pi = eat(SYMBOL, "{", openBraceExpected);
    if (pi.er != none) return pi;

    // Parse varDec*
    while (1) {
        Token look = PeekNextToken();
        if (look.tp == ERR) return makeError(lexerErr, look);
        if (look.tp == RESWORD && strcmp(look.lx, "var") == 0) {
            pi = parseVarDec();
            if (pi.er != none) return pi;
        } else {
            break;
        }
    }

    // Parse statements
    pi = parseStatements();
    if (pi.er != none) return pi;

    pi = eat(SYMBOL, "}", closeBraceExpected);
    return pi;
}

//---------------------------------------
// parseVarDec: var type varName (, varName)* ;
//---------------------------------------
static ParserInfo parseVarDec()
{
    Token first = GetNextToken(); // 'var'
    if (first.tp == ERR) return makeError(lexerErr, first);

    // type
    Token tkType = GetNextToken();
    if (tkType.tp == ERR) return makeError(lexerErr, tkType);
    if (tkType.tp == SYMBOL) {
        return makeError(illegalType, tkType);
    }
    if (tkType.tp == RESWORD) {
        if (strcmp(tkType.lx, "int") && strcmp(tkType.lx, "char") && strcmp(tkType.lx, "boolean")) {
            return makeError(illegalType, tkType);
        }
    } else if (tkType.tp != ID) {
        return makeError(illegalType, tkType);
    }

    // varName
    Token vN = GetNextToken();
    if (vN.tp != ID) {
        return makeError(idExpected, vN);
    }

    while (1) {
        Token look = PeekNextToken();
        if (look.tp == SYMBOL && strcmp(look.lx, ",") == 0) {
            GetNextToken();
            Token nm = GetNextToken();
            if (nm.tp != ID) {
                return makeError(idExpected, nm);
            }
        } else {
            break;
        }
    }

    ParserInfo pi = eat(SYMBOL, ";", semicolonExpected);
    return pi;
}

//---------------------------------------
// parseStatements: statement*
//---------------------------------------
static ParserInfo parseStatements()
{
    ParserInfo pi;
    pi.er = none;
    pi.tk.tp = EOFile;

    while (1) {
        Token look = PeekNextToken();
        if (look.tp == ERR) return makeError(lexerErr, look);
        if (look.tp == SYMBOL && strcmp(look.lx, "}") == 0) break;
        if (look.tp == RESWORD && strcmp(look.lx, "let") == 0) {
            pi = parseLetStatement();
        } else if (look.tp == RESWORD && strcmp(look.lx, "if") == 0) {
            pi = parseIfStatement();
        } else if (look.tp == RESWORD && strcmp(look.lx, "while") == 0) {
            pi = parseWhileStatement();
        } else if (look.tp == RESWORD && strcmp(look.lx, "do") == 0) {
            pi = parseDoStatement();
        } else if (look.tp == RESWORD && strcmp(look.lx, "return") == 0) {
            pi = parseReturnStatement();
        } else {
            return makeError(syntaxError, look);
        }
        if (pi.er != none) return pi;
    }
    return pi;
}

//---------------------------------------
// parseLetStatement: let varName ([expr])? = expr ;
//---------------------------------------
static ParserInfo parseLetStatement()
{
    ParserInfo pi;
    Token letTk = GetNextToken(); // 'let'
    if (letTk.tp == ERR) return makeError(lexerErr, letTk);

    Token varN = GetNextToken();
    if (varN.tp != ID) {
        return makeError(idExpected, varN);
    }

    Token look = PeekNextToken();
    if (look.tp == SYMBOL && strcmp(look.lx, "[") == 0) {
        GetNextToken(); // consume '['
        pi = parseExpression();
        if (pi.er != none) return pi;
        pi = eat(SYMBOL, "]", closeBracketExpected);
        if (pi.er != none) return pi;
    }

    pi = eat(SYMBOL, "=", equalExpected);
    if (pi.er != none) return pi;

    pi = parseExpression();
    if (pi.er != none) return pi;

    pi = eat(SYMBOL, ";", semicolonExpected);
    return pi;
}

static ParserInfo parseIfStatement()
{
    ParserInfo pi;
    Token ifTk = GetNextToken(); // 'if'
    if (ifTk.tp == ERR) return makeError(lexerErr, ifTk);

    pi = eat(SYMBOL, "(", openParenExpected);
    if (pi.er != none) return pi;

    pi = parseExpression();
    if (pi.er != none) return pi;

    pi = eat(SYMBOL, ")", closeParenExpected);
    if (pi.er != none) return pi;

    pi = eat(SYMBOL, "{", openBraceExpected);
    if (pi.er != none) return pi;
    pi = parseStatements();
    if (pi.er != none) return pi;
    pi = eat(SYMBOL, "}", closeBraceExpected);
    if (pi.er != none) return pi;

    Token look = PeekNextToken();
    if (look.tp == RESWORD && strcmp(look.lx, "else") == 0) {
        GetNextToken(); // consume 'else'
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
    ParserInfo pi;
    Token wtk = GetNextToken(); // 'while'
    if (wtk.tp == ERR) return makeError(lexerErr, wtk);

    pi = eat(SYMBOL, "(", openParenExpected);
    if (pi.er != none) return pi;

    pi = parseExpression();
    if (pi.er != none) return pi;

    pi = eat(SYMBOL, ")", closeParenExpected);
    if (pi.er != none) return pi;

    pi = eat(SYMBOL, "{", openBraceExpected);
    if (pi.er != none) return pi;
    pi = parseStatements();
    if (pi.er != none) return pi;
    pi = eat(SYMBOL, "}", closeBraceExpected);
    return pi;
}

static ParserInfo parseDoStatement()
{
    ParserInfo pi;
    Token dtk = GetNextToken(); // 'do'
    if (dtk.tp == ERR) return makeError(lexerErr, dtk);

    Token first = GetNextToken();
    if (first.tp != ID) {
        return makeError(idExpected, first);
    }
    Token look = PeekNextToken();
    if (look.tp == SYMBOL && strcmp(look.lx, ".") == 0) {
        GetNextToken(); // consume '.'
        Token subN = GetNextToken();
        if (subN.tp != ID) {
            return makeError(idExpected, subN);
        }
    }
    pi = eat(SYMBOL, "(", openParenExpected);
    if (pi.er != none) return pi;

    Token l2 = PeekNextToken();
    if (l2.tp != SYMBOL || strcmp(l2.lx, ")") != 0) {
        pi = parseExpression();
        if (pi.er != none) return pi;
        while (1) {
            Token l3 = PeekNextToken();
            if (l3.tp == SYMBOL && strcmp(l3.lx, ",") == 0) {
                GetNextToken();
                pi = parseExpression();
                if (pi.er != none) return pi;
            } else {
                break;
            }
        }
    }
    pi = eat(SYMBOL, ")", closeParenExpected);
    if (pi.er != none) return pi;

    pi = eat(SYMBOL, ";", semicolonExpected);
    return pi;
}

static ParserInfo parseReturnStatement()
{
    ParserInfo pi;
    Token rt = GetNextToken(); // 'return'
    if (rt.tp == ERR) return makeError(lexerErr, rt);

    Token look = PeekNextToken();
    if (!(look.tp == SYMBOL && strcmp(look.lx, ";") == 0)) {
        pi = parseExpression();
        if (pi.er != none) return pi;
    }
    pi = eat(SYMBOL, ";", semicolonExpected);
    return pi;
}

//---------------------------------------
// parseExpression: term (op term)*
//---------------------------------------
static ParserInfo parseExpression()
{
    ParserInfo pi = parseTerm();
    if (pi.er != none) return pi;

    while (1) {
        Token look = PeekNextToken();
        if (look.tp == ERR) return makeError(lexerErr, look);
        if (look.tp == SYMBOL &&
           (!strcmp(look.lx, "+") || !strcmp(look.lx, "-") ||
            !strcmp(look.lx, "*") || !strcmp(look.lx, "/") ||
            !strcmp(look.lx, "&") || !strcmp(look.lx, "|") ||
            !strcmp(look.lx, "<") || !strcmp(look.lx, ">") ||
            !strcmp(look.lx, "="))) 
        {
            GetNextToken(); // consume op
            pi = parseTerm();
            if (pi.er != none) return pi;
        } else {
            break;
        }
    }
    return pi;
}

//---------------------------------------
// parseTerm: minimal term parsing
// If a '}' is encountered unexpectedly, return it for outer function to report error (e.g., semicolonExpected)
//---------------------------------------
static ParserInfo parseTerm()
{
    Token tk = GetNextToken();
    if (tk.tp == ERR) return makeError(lexerErr, tk);

    if (tk.tp == SYMBOL && strcmp(tk.lx, "}") == 0) {
        // Push token back for outer error check
        // For simplicity, we return an error here indicating a missing ";" expected
        return makeError(semicolonExpected, tk);
    }

    if (tk.tp == INT || tk.tp == STRING) {
        ParserInfo pi = { none, tk };
        return pi;
    }
    else if (tk.tp == RESWORD) {
        if (!strcmp(tk.lx, "true") || !strcmp(tk.lx, "false") ||
            !strcmp(tk.lx, "null") || !strcmp(tk.lx, "this")) {
            ParserInfo pi = { none, tk };
            return pi;
        }
        return makeError(syntaxError, tk);
    }
    else if (tk.tp == SYMBOL) {
        if (!strcmp(tk.lx, "(")) {
            ParserInfo pi = parseExpression();
            if (pi.er != none) return pi;
            pi = eat(SYMBOL, ")", closeParenExpected);
            return pi;
        }
        else if (!strcmp(tk.lx, "-") || !strcmp(tk.lx, "~")) {
            return parseTerm();
        }
        else {
            return makeError(syntaxError, tk);
        }
    }
    else if (tk.tp == ID) {
        Token look = PeekNextToken();
        if (look.tp == SYMBOL && strcmp(look.lx, "[") == 0) {
            GetNextToken(); // consume '['
            ParserInfo pi = parseExpression();
            if (pi.er != none) return pi;
            pi = eat(SYMBOL, "]", closeBracketExpected);
            return pi;
        }
        else if (look.tp == SYMBOL && (strcmp(look.lx, "(") == 0 || strcmp(look.lx, ".") == 0)) {
            if (strcmp(look.lx, ".") == 0) {
                GetNextToken(); // consume '.'
                Token subN = GetNextToken();
                if (subN.tp != ID) {
                    return makeError(idExpected, subN);
                }
            }
            ParserInfo pi = eat(SYMBOL, "(", openParenExpected);
            if (pi.er != none) return pi;
            Token look2 = PeekNextToken();
            if (!(look2.tp == SYMBOL && strcmp(look2.lx, ")") == 0)) {
                pi = parseExpression();
                if (pi.er != none) return pi;
                while (1) {
                    Token look3 = PeekNextToken();
                    if (look3.tp == SYMBOL && strcmp(look3.lx, ",") == 0) {
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
            ParserInfo pi = { none, tk };
            return pi;
        }
    }
    else {
        return makeError(syntaxError, tk);
    }
}

//---------------------------------------
// eat: Expect next token to match (tp, lx). Special fixes for expected symbols.
//---------------------------------------
static ParserInfo eat(TokenType tp, const char* lx, SyntaxErrors err)
{
    Token tk = GetNextToken();
    if (tk.tp == ERR) {
        ParserInfo pi;
        pi.er = lexerErr;
        pi.tk = tk;
        return pi;
    }
    // Special fix for semicolon: if expecting ";" but got "}", report semicolonExpected.
    if (tp == SYMBOL && lx && strcmp(lx, ";") == 0) {
        if (tk.tp == SYMBOL && strcmp(tk.lx, ";") != 0) {
            if (strcmp(tk.lx, "}") == 0)
                return makeError(semicolonExpected, tk);
            return makeError(semicolonExpected, tk);
        }
    }
    // Special fix for close paren: if expecting ")" but got something else, report closeParenExpected.
    if (tp == SYMBOL && lx && strcmp(lx, ")") == 0) {
        if (tk.tp == SYMBOL && strcmp(tk.lx, ")") != 0) {
            return makeError(closeParenExpected, tk);
        }
    }
    // Special fix for open paren: if expecting "(" but got something else, report openParenExpected.
    if (tp == SYMBOL && lx && strcmp(lx, "(") == 0) {
        if (tk.tp == SYMBOL && strcmp(tk.lx, "(") != 0) {
            return makeError(openParenExpected, tk);
        }
    }
    if (tk.tp != tp || (lx && strcmp(tk.lx, lx) != 0)) {
        return makeError(err, tk);
    }
    ParserInfo pi;
    pi.er = none;
    pi.tk = tk;
    return pi;
}

//---------------------------------------
// eatAny: Expect next token type tp (ignore lexeme)
//---------------------------------------
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

//---------------------------------------
// makeError: Helper to create ParserInfo error
//---------------------------------------
static ParserInfo makeError(SyntaxErrors er, Token tk)
{
    ParserInfo pi;
    pi.er = er;
    pi.tk = tk;
    return pi;
}

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
            case illegalType: printf("a type must be int, char, boolean, or identifier"); break;
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
