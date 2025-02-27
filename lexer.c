/************************************************************************
University of Leeds
School of Computing
COMP2932- Compiler Design and Construction
Lexer Module

I confirm that the following code has been developed and written by me and it is entirely the result of my own work.
I also confirm that I have not copied any parts of this program from another person or any other source or facilitated someone to copy this program from me.
I confirm that I will not publish the program online or share it with anyone without permission of the module leader.

Student Name:Han Lin
Student ID:201690928
Email:sc22hl@leeds.ac.uk
Date Work Commenced:February 18
*************************************************************************/


static int readChar();
static Token skipWhitespaceAndComments();
static Token readIdentifier();
static Token readNumber();
static Token readString();
static Token readSymbol();

// Initialize the lexer
int InitLexer(char* file_name) {
    strncpy(globalFileName, file_name, sizeof(globalFileName) - 1);
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

// Stop the lexer and clean up
int StopLexer() {
    if (sourceFile) {
        fclose(sourceFile);
        sourceFile = NULL;
    }
    return 1;
}

// Get the next token
Token GetNextToken() {
    if (peeked) {
        peeked = 0;
        return peekToken;
    }

    // Skip whitespace and comments
    Token skipToken = skipWhitespaceAndComments();
    if (skipToken.tp == ERR) {
        return skipToken;
    }

    if (currentChar == EOF) {
        Token t;
        t.tp = EOFile;
        strcpy(t.lx, "EOF");
        t.ec = 0;
        t.ln = currentLine;
        strncpy(t.fl, globalFileName, sizeof(t.fl) - 1);
        return t;
    }

    // Check character type and call the corresponding function
    if (isalpha(currentChar) || currentChar == '_') {
        return readIdentifier();
    } else if (isdigit(currentChar)) {
        return readNumber();
    } else if (currentChar == '"') {
        return readString();
    } else {
        return readSymbol();
    }
}

// Peek the next token without consuming it
Token PeekNextToken() {
    if (!peeked) {
        peekToken = GetNextToken();
        peeked = 1;
    }
    return peekToken;
}

// Read the next character and update line number
static int readChar() {
    int c = fgetc(sourceFile);
    if (c == '\n') {
        currentLine++;
    }
    return c;
}

// Skip whitespace and comments
static Token skipWhitespaceAndComments() {
    Token errToken;
    errToken.tp = EOFile; 
    errToken.lx[0] = '\0';
    errToken.ec = 0;
    errToken.ln = currentLine;
    strncpy(errToken.fl, globalFileName, sizeof(errToken.fl) - 1);

    while (1) {
        while (isspace(currentChar)) {
            currentChar = readChar();
        }
        if (currentChar != '/') {
            break;
        }

        int nextC = readChar();
        if (nextC == '/') {
            // Single-line comment
            while (currentChar != '\n' && currentChar != EOF) {
                currentChar = readChar();
            }
            currentChar = readChar();
        } else if (nextC == '*') {
            // Multi-line comment
            int foundEnd = 0;
            while (!foundEnd) {
                currentChar = readChar();
                if (currentChar == EOF) {
                    Token t;
                    t.tp = ERR;
                    strcpy(t.lx, "Error: unexpected eof in comment");
                    t.ec = EofInCom;
                    t.ln = currentLine;
                    strncpy(t.fl, globalFileName, sizeof(t.fl) - 1);
                    return t;
                } else if (currentChar == '*') {
                    int maybeSlash = readChar();
                    if (maybeSlash == '/') {
                        currentChar = readChar();
                        foundEnd = 1;
                    } else {
                        ungetc(maybeSlash, sourceFile);
                    }
                }
            }
        } else {
            ungetc(nextC, sourceFile);
            break;
        }
    }
    return errToken;
}

// Read an identifier or keyword
static Token readIdentifier() {
    Token t;
    t.ln = currentLine;
    strncpy(t.fl, globalFileName, sizeof(t.fl) - 1);
    t.ec = 0;

    int idx = 0;
    while (isalnum(currentChar) || currentChar == '_') {
        if (idx < (int)sizeof(t.lx) - 1) {
            t.lx[idx++] = (char)currentChar;
        }
        currentChar = readChar();
    }
    t.lx[idx] = '\0';

    t.tp = ID;
    for (int i = 0; i < numKeywords; i++) {
        if (strcmp(t.lx, keywords[i]) == 0) {
            t.tp = RESWORD;
            break;
        }
    }
    return t;
}

// Read a number
static Token readNumber() {
    Token t;
    t.ln = currentLine;
    strncpy(t.fl, globalFileName, sizeof(t.fl) - 1);
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

// Read a string
static Token readString() {
    Token t;
    int startLine = currentLine;

    t.tp = STRING;
    t.ln = startLine;
    strncpy(t.fl, globalFileName, sizeof(t.fl) - 1);
    t.ec = 0;

    currentChar = readChar();
    int idx = 0;

    while (currentChar != '"' && currentChar != EOF) {
        if (currentChar == '\n') {
            t.tp = ERR;
            strcpy(t.lx, "Error: new line in string constant");
            t.ec = NewLnInStr;
            t.ln = startLine;
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
        t.ln = startLine;
        return t;
    }

    t.lx[idx] = '\0';
    currentChar = readChar();
    return t;
}

// Read a symbol or illegal symbol
static Token readSymbol() {
    Token t;
    t.ln = currentLine;
    strncpy(t.fl, globalFileName, sizeof(t.fl) - 1);
    t.ec = 0;

    if (!strchr(allowedSymbols, currentChar)) {
        t.tp = ERR;
        strcpy(t.lx, "Error: illegal symbol in source file");
        t.ec = IllSym;
        currentChar = readChar();
    } else {
        t.tp = SYMBOL;
        t.lx[0] = (char)currentChar;
        t.lx[1] = '\0';
        currentChar = readChar();
    }
    return t;
}

#ifndef TEST
int main(void) {
    char filename[256];
    printf("Enter a JACK file name:\n");
    scanf("%255s", filename);

    if (!InitLexer(filename)) {
        fprintf(stderr, "Cannot open file: %s\n", filename);
        return 1;
    }

    while (1) {
        Token tk = GetNextToken();
        printf("Line: %d, Lexeme: %s, Type: %d\n", tk.ln, tk.lx, tk.tp);
        if (tk.tp == EOFile || tk.tp == ERR) {
            break;
        }
    }

    StopLexer();
    return 0;
}
#endif
