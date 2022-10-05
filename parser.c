#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "mimic.h"

Node *stmt();
Node *expr();
Node *assign();
Node *add();
Node *mul();
Node *unary();
Node *primary();

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

void errorAt(char *loc, char *fmt, ...) {
    int pos = loc - globals.source;
    va_list ap;
    va_start(ap, fmt);

    fprintf(stderr, "%s\n", globals.source);
    if (pos) {
        fprintf(stderr, "%*s", pos, " ");
    }
    fprintf(stderr, "^ ");
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

Token *tokenize() {
    char *p = globals.source;
    Token head;
    head.next = NULL;
    Token *current = &head;

    while (*p) {
        if (isSpace(*p)) {
            ++p;
            continue;
        }
        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' || *p == ')' || *p == '=' || *p == ';') {
            current = newToken(TokenReserved, current, p++);
            continue;
        }
        if (isDigit(*p)) {
            current = newToken(TokenNumber, current, p);
            current->val = strtol(p, &p, 10);
            continue;
        }
        if ('a' <= *p && *p <= 'z') {
            current = newToken(TokenIdent, current, p++);
            // TODO: set token length
            continue;
        }

        errorAt(p, "Cannot tokenize");
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

// If the type of the current token is TokenIdent, consume the token and
// returns the pointer to token structure.  If not, returns NULL instead.
Token *consumeIdent() {
    if (globals.token->type == TokenIdent) {
        Token *t = globals.token;
        globals.token = globals.token->next;
        return t;
    }
    return NULL;
}

// If the current token is the expected sign, consume the token. Otherwise
// reports an error.
void expectSign(char op) {
    if (globals.token->type == TokenReserved && *globals.token->str == op) {
        globals.token = globals.token->next;
        return;
    }
    errorAt(globals.token->str, "'%c' is expected.", op);
}

// If the current token is the number token, consume the token and returns the
// value. Otherwise reports an error.
int expectNumber() {
    if (globals.token->type == TokenNumber) {
        int val = globals.token->val;
        globals.token = globals.token->next;
        return val;
    }
    errorAt(globals.token->str, "Non number appears.");
    return 0;
}

int atEOF() {
    return globals.token->type == TokenEOF;
}

Node *newNode(NodeKind kind, Node *lhs, Node *rhs) {
    Node *n = calloc(1, sizeof(Node));
    n->kind = kind;
    n->lhs =  lhs;
    n->rhs =  rhs;
    return n;
}

Node *newNodeNum(int val) {
    Node *n = newNode(NodeNum, NULL, NULL);
    n->val = val;
    return n;
}

// TODO: Argument should be `int offset` instead of `char charIdx`
Node *newNodeLVar(char charIdx) {
    Node *n = newNode(NodeLVar, NULL, NULL);
    n->offset = (int)charIdx * 8; // 8bytes for one variable (temporally).
    return n;
}

void program() {
    int i = 0;
    while (!atEOF()) {
        globals.code[i++] = stmt();
        if (i >= 100) { // TODO: Remove magic number
            error("Too many expressions. Hanged.");
        }
    }
    globals.code[i] = NULL;
}

Node *stmt() {
    Node *n = expr();
    expectSign(';');
    return n;
}

Node *expr() {
    Node *n = assign();
    return n;
}

Node *assign() {
    Node *n = add();
    if (consumeToken('=')) {
        n = newNode(NodeAssign, n, assign());
    }
    return n;
}

Node *add() {
    Node *n = mul();
    for (;;) {
        if (consumeToken('+')) {
            n = newNode(NodeAdd, n, mul());
        } else if (consumeToken('-')) {
            n = newNode(NodeSub, n, mul());
        } else {
            return n;
        }
    }
}

Node *mul() {
    Node *n = unary();
    for (;;) {
        if (consumeToken('*')) {
            n = newNode(NodeMul, n, unary());
        } else if (consumeToken('/')) {
            n = newNode(NodeDiv, n, unary());
        } else {
            return n;
        }
    }
}

Node *unary() {
    if (consumeToken('+')) {
        return newNode(NodeAdd, newNodeNum(0), primary());
    } else if (consumeToken('-')) {
        return newNode(NodeSub, newNodeNum(0), primary());
    } else {
        return primary();
    }
}

Node *primary() {
    if (consumeToken('(')) {
        Node *n = expr();
        expectSign(')');
        return n;
    }

    Token *t = consumeIdent();
    if (t) {
        return newNodeLVar(t->str[0] - 'a' + 1);
    }
    return newNodeNum(expectNumber());
}
