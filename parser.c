#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "lexer.h"
#include "parser.h"

// Forward declarations
static ParserInfo parseClass();
static ParserInfo parseClassVarDec();
static ParserInfo parseSubroutineDec();
static ParserInfo parseOptionalStatementsUntilBrace(); // minimal skip logic

static int parserInited = 0;

// Initialize the parser
int InitParser(char* file_name)
{
    // Call the lexer init so we can get tokens from the same file
    if (!InitLexer(file_name)) {
        return 0; // if lexer can't open file
    }
    parserInited = 1;
    return 1;
}

// Main parse function
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

    // parse the class
    pi = parseClass();
    if (pi.er != none) {
        return pi;
    }

    // If no error, optionally check if there's leftover tokens
    // For example, we read next token to see if it's EOF
    Token tk = GetNextToken();
    if (tk.tp != EOFile && tk.tp != ERR) {
        // We'll ignore leftover content (non-fatal),
        // but you could treat it as an error if you want.
        // E.g.:
        // pi.er = syntaxError;
        // pi.tk = tk;
    }
    else if (tk.tp == ERR) {
        pi.er = lexerErr;
        pi.tk = tk;
    }

    return pi;
}

// Stop the parser
int StopParser()
{
    StopLexer(); 
    parserInited = 0;
    return 1;
}

// ----------------------------
// parseClass:
// Expects: class <id> { (classVarDec)* (subroutineDec)* }
// ----------------------------
static ParserInfo parseClass()
{
    ParserInfo pi;
    pi.er = none;
    pi.tk.tp = EOFile;

    // 1) 'class'
    Token tk = GetNextToken();
    if (tk.tp == ERR) {
        pi.er = lexerErr;
        pi.tk = tk;
        return pi;
    }
    if (tk.tp != RESWORD || strcmp(tk.lx, "class") != 0) {
        pi.er = classExpected;
        pi.tk = tk;
        return pi;
    }

    // 2) class name (identifier)
    tk = GetNextToken();
    if (tk.tp == ERR) {
        pi.er = lexerErr;
        pi.tk = tk;
        return pi;
    }
    if (tk.tp != ID) {
        pi.er = idExpected;
        pi.tk = tk;
        return pi;
    }

    // 3) '{'
    tk = GetNextToken();
    if (tk.tp == ERR) {
        pi.er = lexerErr;
        pi.tk = tk;
        return pi;
    }
    if (tk.tp != SYMBOL || strcmp(tk.lx, "{") != 0) {
        pi.er = openBraceExpected;
        pi.tk = tk;
        return pi;
    }

    // 4) parse classVarDec* => field | static
    while (1) {
        Token look = PeekNextToken();
        if (look.tp == ERR) {
            pi.er = lexerErr;
            pi.tk = look;
            return pi;
        }
        // break if '}' or subroutine keywords or we can't parse it
        if (look.tp == SYMBOL && strcmp(look.lx, "}") == 0) {
            break;
        }
        if (look.tp == RESWORD && 
            (strcmp(look.lx, "field") == 0 || strcmp(look.lx, "static") == 0)) 
        {
            // parse a classVarDec
            pi = parseClassVarDec();
            if (pi.er != none) return pi;
        } 
        else {
            // not a classVarDec => maybe subroutine dec or end
            break;
        }
    }

    // 5) parse subroutineDec* => constructor | function | method
    while (1) {
        Token look = PeekNextToken();
        if (look.tp == ERR) {
            pi.er = lexerErr;
            pi.tk = look;
            return pi;
        }
        // break if '}' or not a subroutine keyword
        if (look.tp == SYMBOL && strcmp(look.lx, "}") == 0) {
            break;
        }
        if (look.tp == RESWORD && 
            (strcmp(look.lx, "constructor") == 0 || 
             strcmp(look.lx, "function") == 0 ||
             strcmp(look.lx, "method") == 0)) 
        {
            // parse a subroutine
            pi = parseSubroutineDec();
            if (pi.er != none) return pi;
        }
        else {
            // not subroutine => assume end
            break;
        }
    }

    // 6) final '}'
    tk = GetNextToken();
    if (tk.tp == ERR) {
        pi.er = lexerErr;
        pi.tk = tk;
        return pi;
    }
    if (tk.tp != SYMBOL || strcmp(tk.lx, "}") != 0) {
        pi.er = closeBraceExpected;
        pi.tk = tk;
        return pi;
    }

    return pi;
}

// ----------------------------
// parseClassVarDec:
// field|static type varName (',' varName)* ';'
// For simplicity, we do minimal checks
// ----------------------------
static ParserInfo parseClassVarDec()
{
    ParserInfo pi;
    pi.er = none;
    pi.tk.tp = EOFile;

    // we've already peeked that next is field|static
    Token tk = GetNextToken(); // consume field|static
    if (tk.tp == ERR) {
        pi.er = lexerErr;
        pi.tk = tk;
        return pi;
    }
    // read type (int|char|boolean|identifier)
    tk = GetNextToken();
    if (tk.tp == ERR) {
        pi.er = lexerErr;
        pi.tk = tk;
        return pi;
    }
    // minimal check
    if (!(tk.tp == RESWORD && 
          (strcmp(tk.lx,"int")==0 || strcmp(tk.lx,"char")==0 || strcmp(tk.lx,"boolean")==0))
        && tk.tp != ID) 
    {
        pi.er = illegalType;
        pi.tk = tk;
        return pi;
    }

    // read varName
    tk = GetNextToken();
    if (tk.tp == ERR) {
        pi.er = lexerErr;
        pi.tk = tk;
        return pi;
    }
    if (tk.tp != ID) {
        pi.er = idExpected;
        pi.tk = tk;
        return pi;
    }

    // check for more varName (comma-separated)
    while (1) {
        Token look = PeekNextToken();
        if (look.tp == SYMBOL && strcmp(look.lx, ",") == 0) {
            GetNextToken(); // consume ','
            // read next varName
            Token nameTk = GetNextToken();
            if (nameTk.tp != ID) {
                pi.er = idExpected;
                pi.tk = nameTk;
                return pi;
            }
        } else {
            break;
        }
    }

    // expect ';'
    tk = GetNextToken();
    if (tk.tp != SYMBOL || strcmp(tk.lx, ";") != 0) {
        pi.er = semicolonExpected;
        pi.tk = tk;
        return pi;
    }

    return pi;
}

// ----------------------------
// parseSubroutineDec:
// constructor|function|method (void|type) subroutineName '(' paramList ')' subroutineBody
// We skip subroutineBody's detailed parse here
// ----------------------------
static ParserInfo parseSubroutineDec()
{
    ParserInfo pi;
    pi.er = none;
    pi.tk.tp = EOFile;

    // we've already peeked that next is constructor|function|method
    Token tk = GetNextToken(); // consume
    if (tk.tp == ERR) {
        pi.er = lexerErr;
        pi.tk = tk;
        return pi;
    }

    // return type
    tk = GetNextToken();
    if (tk.tp == ERR) {
        pi.er = lexerErr;
        pi.tk = tk;
        return pi;
    }
    // minimal check: (void|int|char|boolean|ID)
    if (!(tk.tp == RESWORD && (strcmp(tk.lx,"void")==0 
         || strcmp(tk.lx,"int")==0 || strcmp(tk.lx,"char")==0 || strcmp(tk.lx,"boolean")==0))
        && tk.tp != ID)
    {
        pi.er = illegalType;
        pi.tk = tk;
        return pi;
    }

    // subroutine name
    tk = GetNextToken();
    if (tk.tp == ERR) {
        pi.er = lexerErr;
        pi.tk = tk;
        return pi;
    }
    if (tk.tp != ID) {
        pi.er = idExpected;
        pi.tk = tk;
        return pi;
    }

    // '('
    tk = GetNextToken();
    if (tk.tp != SYMBOL || strcmp(tk.lx, "(") != 0) {
        pi.er = openParenExpected;
        pi.tk = tk;
        return pi;
    }
    // skip paramList for now - we do minimal approach
    // we will read tokens until we find ')'
    while (1) {
        Token look = PeekNextToken();
        if (look.tp == ERR) {
            pi.er = lexerErr;
            pi.tk = look;
            return pi;
        }
        if (look.tp == SYMBOL && strcmp(look.lx, ")") == 0) {
            GetNextToken(); // consume ')'
            break;
        }
        // consume and skip
        GetNextToken();
        if (look.tp == EOFile) {
            pi.er = closeParenExpected;
            pi.tk = look;
            return pi;
        }
    }

    // subroutineBody: expect '{'
    tk = GetNextToken();
    if (tk.tp != SYMBOL || strcmp(tk.lx, "{") != 0) {
        pi.er = openBraceExpected;
        pi.tk = tk;
        return pi;
    }

    // minimal skip until we find matching '}'
    pi = parseOptionalStatementsUntilBrace();
    if (pi.er != none) {
        return pi;
    }

    return pi; 
}

// minimal skip logic for subroutine body
static ParserInfo parseOptionalStatementsUntilBrace()
{
    ParserInfo pi;
    pi.er = none;
    pi.tk.tp = EOFile;

    while (1) {
        Token look = PeekNextToken();
        if (look.tp == ERR) {
            pi.er = lexerErr;
            pi.tk = look;
            return pi;
        }
        if (look.tp == EOFile) {
            // didn't find closing brace
            pi.er = closeBraceExpected;
            pi.tk = look;
            return pi;
        }
        if (look.tp == SYMBOL && strcmp(look.lx,"}") == 0) {
            // found closing brace
            GetNextToken(); // consume
            break;
        }
        // skip
        GetNextToken();
    }
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
        printf("Parse error at line %d, near '%s'. Error code = %d\n",
               result.tk.ln, result.tk.lx, result.er);
    }

    StopParser();
    return 0;
}
#endif
