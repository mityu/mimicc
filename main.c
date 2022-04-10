#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

typedef enum {
    TokenReserved,
    TokenNumber,
    TokenEOF,
} TokenType;

typedef struct Token Token;
struct Token {
    TokenType type;
    Token *next;
    int val;    // The number when type == TokenNumber.
    char *str;  // The token string
};

struct Globals {
    Token* token;  // The token currently watches
} globals;


int isSpace(char c) {
    return c == ' ' || c == '\n' || c == '\t';
}

int isDigit(char c) {
    c = c - '0';
    return 0 <= c && c <= 9;
}


// Print error message on stderr and exit.
void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// Generate a new token, concatenate it to `current`, and return it.
Token *newToken(TokenType type, Token *current, char *str) {
    Token *t = (Token*)calloc(1, sizeof(Token));
    t->type = type;
    t->str = str;
    current->next = t;
    return t;
}

Token *tokenize(char* p) {
    Token head;
    head.next = NULL;
    Token *current = &head;

    while (*p) {
        if (isSpace(*p)) {
            ++p;
            continue;
        }
        if (*p == '+' || *p == '-') {
            current = newToken(TokenReserved, current, p++);
            continue;
        }
        if (isDigit(*p)) {
            current = newToken(TokenNumber, current, p);
            current->val = strtol(p, &p, 10);
            continue;
        }

        error("Cannot tokenize: %s", p);
    }

    newToken(TokenEOF, current, p);

    return head.next;
}

// If the current token is the expected token, consume the token and returns
// TRUE.
int consumeToken(char op) {
    if (globals.token->type == TokenReserved && *globals.token->str == op) {
        globals.token = globals.token->next;
        return 1;
    }
    return 0;
}

// If the current token is the expected sign, consume the token. Otherwise
// reports an error.
void expectSign(char op) {
    if (globals.token->type == TokenReserved && *globals.token->str == op) {
        globals.token = globals.token->next;
        return;
    }
    error("'%c' is expected, but not.", op);
}

// If the current token is the number token, consume the token and returns the
// value. Otherwise reports an error.
int expectNumber() {
    if (globals.token->type == TokenNumber) {
        int val = globals.token->val;
        globals.token = globals.token->next;
        return val;
    }
    error("Non number appears.");
    return 0;
}

int atEOF() {
    return globals.token->type == TokenEOF;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Invalid arguments\n");
        return 1;
    }

    globals.token = tokenize(argv[1]);

    puts(".intel_syntax noprefix");
    puts(".globl main");
    puts("main:");

    printf("  mov rax, %d\n", expectNumber());
    while (!atEOF()) {
        if (consumeToken('+')) {
            printf("  add rax, %d\n", expectNumber());
            continue;
        }
        expectSign('-');
        printf("  sub rax, %d\n", expectNumber());
    }

    puts("  ret");
    return 0;
}
