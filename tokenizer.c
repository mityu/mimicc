#include <stdlib.h>
#include <string.h>
#include "mimic.h"

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
static Token *newToken(TokenType type, Token *current, char *str, int len) {
    Token *t = (Token*)safeAlloc(sizeof(Token));
    t->type = type;
    t->str = str;
    t->len = len;
    t->prev = current;
    current->next = t;
    return t;
}

Token *tokenize(void) {
    char *p = globals.source;
    Token head;
    head.next = NULL;
    Token *current = &head;

    while (*p) {
        if (isSpace(*p)) {
            ++p;
            continue;
        }

        if (hasPrefix(p, "/*")) {
            char *q = strstr(p + 2, "*/");
            if (q == NULL)
                errorAt(p, "Unterminated comment");
            p = q + 2;
            continue;
        }

        if (hasPrefix(p, "//")) {
            p += 2;
            while (*p != '\n' && *p != '\0')
                ++p;
            continue;
        }

        if (isToken(p, "void")) {
            current = newToken(TokenTypeName, current, p, 4);
            current->varType = TypeVoid;
            p += 4;
            continue;
        }

        if (isToken(p, "int")) {
            current = newToken(TokenTypeName, current, p, 3);
            current->varType = TypeInt;
            p += 3;
            continue;
        }

        if (isToken(p, "char")) {
            current = newToken(TokenTypeName, current, p, 4);
            current->varType = TypeChar;
            p += 4;
            continue;
        }

        if (isToken(p, "struct")) {
            current = newToken(TokenStruct, current, p, 6);
            p += 6;
            continue;
        }

        if (isToken(p, "const")) {
            // TODO: Create new token; Take into account when parsing.
            p += 5;
            continue;
        }

        if (isToken(p, "if")) {
            current = newToken(TokenIf, current, p, 2);
            p += 2;
            continue;
        }

        if (isToken(p, "else")) {
            char *q = p + 5;
            while (*q && isSpace(*q))
                ++q;
            if (isToken(q, "if")) {
                q += 2;
                current = newToken(TokenElseif, current, p, q - p);
                p = q;
            } else {
                current = newToken(TokenElse, current, p, 4);
                p += 4;
            }
            continue;
        }

        if (isToken(p, "for")) {
            current = newToken(TokenFor, current, p, 3);
            p += 3;
            continue;
        }

        if (isToken(p, "while")) {
            current = newToken(TokenWhile, current, p, 5);
            p += 5;
            continue;
        }

        if (isToken(p, "break")) {
            current = newToken(TokenBreak, current, p, 5);
            p += 5;
            continue;
        }

        if (isToken(p, "continue")) {
            current = newToken(TokenContinue, current, p, 8);
            p += 8;
            continue;
        }

        if (isToken(p, "return")) {
            current = newToken(TokenReturn, current, p, 6);
            p += 6;
            continue;
        }

        if (isToken(p, "sizeof")) {
            current = newToken(TokenSizeof, current, p, 6);
            p += 6;
            continue;
        }

        if (hasPrefix(p, "...")) {
            current = newToken(TokenReserved, current, p, 3);
            p += 3;
            continue;
        }

        if (hasPrefix(p, "==") || hasPrefix(p, "!=") ||
                hasPrefix(p, ">=") || hasPrefix(p, "<=")  ||
                hasPrefix(p, "+=") || hasPrefix(p, "-=") ||
                hasPrefix(p, "++") || hasPrefix(p, "--") ||
                hasPrefix(p, "&&") || hasPrefix(p, "||") ||
                hasPrefix(p, "->")) {
            current = newToken(TokenReserved, current, p, 2);
            p += 2;
            continue;
        }
        if (strchr("!+-*/%()=;[]<>{},&.", *p)) {
            current = newToken(TokenReserved, current, p++, 1);
            continue;
        }
        if (isDigit(*p)) {
            current = newToken(TokenNumber, current, p, 0);
            char *q = p;
            current->val = strtol(p, &p, 10);
            current->len = p - q;
            continue;
        }

        if (*p == '\'') {
            char *q = p;
            char c;
            ++p;
            if (*p == '\0') {
                errorAt(p, "Character literal is not terminated.");
            } else if (*p == '\'') {
                errorAt(q, "Empty character literal.");
            } else if (*p == '\\') {
                if (!checkEscapeChar(*(++p), &c)) {
                    errorAt(p - 1, "Invalid escape character.");
                }
            } else {
                c = *p;
            }

            if (*(++p) != '\'') {
                errorAt(p, "Character literal is too long.");
            }
            current = newToken(TokenNumber, current, q, p - q + 1);
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
                        errorAt(p - 1, "Invalid escape character.");
                    }
                }
            }

            if (*p == '\0')
                errorAt(p - 1, "String is not terminated.");

            str = (LiteralString *)safeAlloc(sizeof(LiteralString));
            str->id = globals.literalStringCount++;
            str->len = len;
            str->string = (char *)malloc(literalLen * sizeof(char));
            memcpy(str->string, q + 1, literalLen - 1);
            str->string[literalLen - 1] = '\0';
            str->next = globals.strings;
            globals.strings = str;

            current = newToken(TokenLiteralString, current, q, p - q + 1);
            current->literalStr = str;

            p++;  // Skip closing double quote.
            continue;
        }

        if ('a' <= *p && *p <= 'z' || 'A' <= *p && *p <= 'Z' || *p == '_') {
            char *q = p;
            while (isAlnum(*p))
                ++p;
            current = newToken(TokenIdent, current, q, p - q);
            continue;
        }

        errorAt(p, "Cannot tokenize");
    }

    newToken(TokenEOF, current, p, 0);

    return head.next;
}
