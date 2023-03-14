#include <stdlib.h>
#include <string.h>
#include "mimicc.h"

static int isSpace(char c) {
    return c == ' ' || c == '\n' || c == '\t';
}

static int isDigit(char c) {
    c = c - '0';
    return 0 <= c && c <= 9;
}

static int isAlnum(char c) {
    if (('a' <= c && c <= 'z') ||
            ('A' <= c && c <= 'Z') ||
            ('0' <= c && c <= '9') ||
            c == '_')
        return 1;
    return 0;
}

static int hasPrefix(const char *s1, const char *s2) {
    return strncmp(s1, s2, strlen(s2)) == 0;
}

static int isToken(char *p, char *op) {
    return hasPrefix(p, op) && !isAlnum(p[strlen(op)]);
}

// Check character after backslash builds an escape character.
// If so, set the escape character to *decoded and returns TRUE.
int checkEscapeChar(char c, char *decoded) {
    static const char table[][2] = {
        {'\'', '\''},
        {'"', '"'},
        {'\\', '\\'},
        {'?', '\?'},
        {'a', '\a'},
        {'b', '\b'},
        {'f', '\f'},
        {'n', '\n'},
        {'r', '\r'},
        {'t', '\t'},
        {'v', '\v'},
        {'0', '\0'},
    };
    for (int i = 0; i < (sizeof(table)/sizeof(table[0])); ++i) {
        if (c == table[i][0]) {
            *decoded = table[i][1];
            return 1;
        }
    }
    *decoded = c;
    return 0;
}

// Generate a new token, concatenate it to `current`, and return the new one.
static Token *newToken(TokenType type, Token *current, char *str, int len, int line, int column) {
    Token *t = (Token*)safeAlloc(sizeof(Token));
    t->type = type;
    t->str = str;
    t->len = len;
    t->prev = current;
    t->line = line;
    t->column = column;
    current->next = t;
    return t;
}

// Remove tokens from "token" by range [begin, end].
void popTokenRange(Token *begin, Token *end) {
    Token *prev = begin->prev;
    Token *next = end->next;

    if (!(prev && next))
        errorUnreachable();

    prev->next = next;
    next->prev = prev;
}

// Remove all tokens whose type is TokenNewLine from "token".
void removeAllNewLineToken(Token *token) {
    Token *cur = token;
    while (cur->type != TokenEOF) {
        if (cur->type == TokenNewLine) {
            Token *next = cur->next;
            popTokenRange(cur, cur);
            cur = next;
        } else {
            cur = cur->next;
        }
    }
}

void printToken(Token *token) {
    if (!token) {
        puts("(NULL)");
        return;
    }

    switch (token->type) {
    case TokenReserved:
        printf("RESERVED: %.*s\n", token->len, token->str);
        break;
    case TokenTypeName:
        printf("TYPENAME: %.*s\n", token->len, token->str);
        break;
    case TokenIdent:
        printf("IDENT   : %.*s\n", token->len, token->str);
        break;
    case TokenNumber:
        printf("NUMBER  : %d\n", token->val);
        break;
    case TokenStatic:
        puts("static");
        break;
    case TokenExtern:
        puts("extern");
        break;
    case TokenTypedef:
        puts("typedef");
        break;
    case TokenIf:
        puts("if");
        break;
    case TokenElseif:
        puts("else-if");
        break;
    case TokenElse:
        puts("else");
        break;
    case TokenSwitch:
        puts("switch");
        break;
    case TokenCase:
        puts("case");
        break;
    case TokenDefault:
        puts("default");
        break;
    case TokenFor:
        puts("for");
        break;
    case TokenWhile:
        puts("while");
        break;
    case TokenDo:
        puts("do");
        break;
    case TokenBreak:
        puts("break");
        break;
    case TokenContinue:
        puts("continue");
        break;
    case TokenReturn:
        puts("return");
        break;
    case TokenSizeof:
        puts("sizeof");
        break;
    case TokenLiteralString:
        printf("STRING  : %s\n", token->literalStr->string);
        break;
    case TokenStruct:
        puts("struct");
        break;
    case TokenEnum:
        puts("enum");
        break;
    case TokenNewLine:
        puts("NEWLINE");
        break;
    case TokenSOF:
        puts("===START OF FILE===");
        break;
    case TokenEOF:
        puts("===END OF FILE===");
        break;
    }
}

void printTokenList(Token *token) {
    for (; token; token = token->next)
        printToken(token);
}

#define appendNewToken(tokenType, str, len) \
    do {\
        current = newToken(\
                tokenType, current, str, len, line, (int)((str) - lineHead));\
    } while (0)
#define errorAtChar(pos, msg) \
    do { \
        appendNewToken(TokenReserved, pos, 0);\
        errorAt(current, msg);\
    } while (0)
Token *tokenize(char *source) {
    char *p = source;
    Token head = {};
    Token *current = &head;
    int line = 1;
    char *lineHead = p;

    appendNewToken(TokenSOF, p, 0);

    while (*p) {
        if (*p == '\n') {
            appendNewToken(TokenNewLine, p, 1);
            ++p;
            ++line;
            lineHead = p;
            continue;
        }

        if (isSpace(*p)) {
            ++p;
            continue;
        }

        if (hasPrefix(p, "/*")) {
            char *q = strstr(p + 2, "*/");
            if (q == NULL)
                errorAtChar(p, "Unterminated comment");
            p = q + 2;
            continue;
        }

        if (hasPrefix(p, "//")) {
            p += 2;
            while (*p != '\n' && *p != '\0')
                ++p;
            continue;
        }

        if (isToken(p, "_Noreturn")) {
            // Just ignore it now.
            p += 9;
            continue;
        }

        if (isToken(p, "void")) {
            appendNewToken(TokenTypeName, p, 4);
            current->varType = TypeVoid;
            p += 4;
            continue;
        }

        if (isToken(p, "int")) {
            appendNewToken(TokenTypeName, p, 3);
            current->varType = TypeInt;
            p += 3;
            continue;
        }

        if (isToken(p, "char")) {
            appendNewToken(TokenTypeName, p, 4);
            current->varType = TypeChar;
            p += 4;
            continue;
        }

        if (isToken(p, "struct")) {
            appendNewToken(TokenStruct, p, 6);
            p += 6;
            continue;
        }

        if (isToken(p, "enum")) {
            appendNewToken(TokenEnum, p, 4);
            p += 4;
            continue;
        }

        if (isToken(p, "const")) {
            // TODO: Create new token; Take into account when parsing.
            p += 5;
            continue;
        }

        if (isToken(p, "static")) {
            appendNewToken(TokenStatic, p, 6);
            p += 6;
            continue;
        }

        if (isToken(p, "extern")) {
            appendNewToken(TokenExtern, p, 6);
            p += 6;
            continue;
        }

        if (isToken(p, "typedef")) {
            appendNewToken(TokenTypedef, p, 7);
            p += 7;
            continue;
        }

        if (isToken(p, "if")) {
            appendNewToken(TokenIf, p, 2);
            p += 2;
            continue;
        }

        if (isToken(p, "else")) {
            char *q = p + 5;
            while (*q && isSpace(*q))
                ++q;
            if (isToken(q, "if")) {
                q += 2;
                appendNewToken(TokenElseif, p, (int)(q - p));
                p = q;
            } else {
                appendNewToken(TokenElse, p, 4);
                p += 4;
            }
            continue;
        }

        if (isToken(p, "switch")) {
            appendNewToken(TokenSwitch, p, 6);
            p += 6;
            continue;
        }

        if (isToken(p, "case")) {
            appendNewToken(TokenCase, p, 4);
            p += 4;
            continue;
        }

        if (isToken(p, "default")) {
            appendNewToken(TokenDefault, p, 7);
            p += 7;
            continue;
        }

        if (isToken(p, "for")) {
            appendNewToken(TokenFor, p, 3);
            p += 3;
            continue;
        }

        if (isToken(p, "while")) {
            appendNewToken(TokenWhile, p, 5);
            p += 5;
            continue;
        }

        if (isToken(p, "do")) {
            appendNewToken(TokenDo, p, 2);
            p += 2;
            continue;
        }

        if (isToken(p, "break")) {
            appendNewToken(TokenBreak, p, 5);
            p += 5;
            continue;
        }

        if (isToken(p, "continue")) {
            appendNewToken(TokenContinue, p, 8);
            p += 8;
            continue;
        }

        if (isToken(p, "return")) {
            appendNewToken(TokenReturn, p, 6);
            p += 6;
            continue;
        }

        if (isToken(p, "sizeof")) {
            appendNewToken(TokenSizeof, p, 6);
            p += 6;
            continue;
        }

        if (hasPrefix(p, "...")) {
            appendNewToken(TokenReserved, p, 3);
            p += 3;
            continue;
        }

        if (hasPrefix(p, "==") || hasPrefix(p, "!=") ||
                hasPrefix(p, ">=") || hasPrefix(p, "<=")  ||
                hasPrefix(p, "+=") || hasPrefix(p, "-=") ||
                hasPrefix(p, "*=") || hasPrefix(p, "/=") ||
                hasPrefix(p, "&=") || hasPrefix(p, "|=") ||
                hasPrefix(p, "^=") ||
                hasPrefix(p, "++") || hasPrefix(p, "--") ||
                hasPrefix(p, "&&") || hasPrefix(p, "||") ||
                hasPrefix(p, "<<") || hasPrefix(p, ">>") ||
                hasPrefix(p, "->")) {
            appendNewToken(TokenReserved, p, 2);
            p += 2;
            continue;
        }
        if (strchr("!+-*/%()=;[]<>{},&^|.?:#", *p)) {
            appendNewToken(TokenReserved, p, 1);
            p++;
            continue;
        }
        if (isDigit(*p)) {
            appendNewToken(TokenNumber, p, 0);
            char *q = p;
            current->val = strtol(p, &p, 10);
            current->len = (int)(p - q);
            continue;
        }

        if (*p == '\'') {
            char *q = p;
            char c;
            ++p;
            if (*p == '\0') {
                errorAtChar(p, "Character literal is not terminated.");
            } else if (*p == '\'') {
                errorAtChar(q, "Empty character literal.");
            } else if (*p == '\\') {
                if (!checkEscapeChar(*(++p), &c)) {
                    errorAtChar(p - 1, "Invalid escape character.");
                }
            } else {
                c = *p;
            }

            if (*(++p) != '\'') {
                errorAtChar(p, "Character literal is too long.");
            }
            appendNewToken(TokenNumber, q, (int)(p - q + 1));
            current->val = (int)c;
            p++;
            continue;
        }

        if (*p == '"') {
            char *q = p;
            int literalLen = 0;  // String length on text editor.
            int len = 0;  // String length in program.
            LiteralString *str = NULL;

            while (*(++p) != '\0') {
                ++len;
                ++literalLen;
                if (*p == '"') {
                    break;
                } else if (*p == '\\') {
                    char c;
                    ++p;
                    ++literalLen;
                    if (*p == '\0') {
                        --literalLen;
                        break;
                    } else if (!checkEscapeChar(*p, &c)) {
                        errorAtChar(p - 1, "Invalid escape character.");
                    }
                }
            }

            if (*p == '\0')
                errorAtChar(p - 1, "String is not terminated.");

            str = (LiteralString *)safeAlloc(sizeof(LiteralString));
            str->id = globals.literalStringCount++;
            str->len = len;
            str->string = (char *)malloc(literalLen * sizeof(char));
            memcpy(str->string, q + 1, literalLen - 1);
            str->string[literalLen - 1] = '\0';
            str->next = globals.strings;
            globals.strings = str;

            appendNewToken(TokenLiteralString, q, (int)(p - q + 1));
            current->literalStr = str;

            p++;  // Skip closing double quote.
            continue;
        }

        if ('a' <= *p && *p <= 'z' || 'A' <= *p && *p <= 'Z' || *p == '_') {
            char *q = p;
            while (isAlnum(*p))
                ++p;
            appendNewToken(TokenIdent, q, (int)(p - q));
            continue;
        }

        errorAtChar(p, "Cannot tokenize");
    }

    appendNewToken(TokenEOF, p, 0);

    return head.next;
}
