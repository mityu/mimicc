#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "mimic.h"

static Token *newToken(TokenType type, Token *current, char *str, int len);
static Node *stmt();
static Node *expr();
static Node *assign();
static Node *equality();
static Node *relational();
static Node *add();
static Node *mul();
static Node *unary();
static Node *primary();

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

static int hasPrefix(char *s1, char *s2) {
    return strncmp(s1, s2, strlen(s2)) == 0;
}


// Print error message on stderr and exit.
void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

static void errorAt(char *loc, char *fmt, ...) {
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

// If `p` has the control syntax tokenifier `token` at the head, skip `token`,
// creates new token, and returns TRUE.  Otherwise just returns FALSE.
// static int tokenizeIdentToken(char *p, char *token, TokenType type, Token *current) {
//     size_t tlen = strlen(token);
//     if (hasPrefix(p, token) && !isAlnum(token[tlen])) {
//         current = newToken(type, current, p, tlen);
//         p += tlen;
//         return 1;
//     }
//     return 0;
// }

// Generate a new token, concatenate it to `current`, and return the new one.
static Token *newToken(TokenType type, Token *current, char *str, int len) {
    Token *t = (Token*)calloc(1, sizeof(Token));
    t->type = type;
    t->str = str;
    t->len = len;
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

        if (hasPrefix(p, "if") && !isAlnum(p[2])) {
            current = newToken(TokenIf, current, p, 2);
            p += 2;
            continue;
        }

        if (hasPrefix(p, "return") && !isAlnum(p[6])) {
            current = newToken(TokenReturn, current, p, 6);
            p += 6;
            continue;
        }
        if (hasPrefix(p, "==") || hasPrefix(p, "!=") ||
                hasPrefix(p, ">=") || hasPrefix(p, "<=")) {
            current = newToken(TokenReserved, current, p, 2);
            p += 2;
            continue;
        }
        if (strchr("+-*/()=;<>", *p)) {
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
        if ('a' <= *p && *p <= 'z' || *p == '_') {
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

// If the current token is the expected token, consume the token and returns
// TRUE.
static int consumeToken(char *op) {
    if (globals.token->type == TokenReserved &&
            strlen(op) == (size_t)globals.token->len &&
            memcmp(globals.token->str, op, (size_t)globals.token->len) == 0) {
        globals.token = globals.token->next;
        return 1;
    }
    return 0;
}

// If the type of the current token is TokenIdent, consume the token and
// returns the pointer to token structure.  If not, returns NULL instead.
static Token *consumeIdent() {
    if (globals.token->type == TokenIdent) {
        Token *t = globals.token;
        globals.token = globals.token->next;
        return t;
    }
    return NULL;
}

// If the type of the current token is TokenReturn, consume the token and
// returns TRUE.
static int consumeReturn() {
    if (globals.token->type == TokenReturn) {
        globals.token = globals.token->next;
        return 1;
    }
    return 0;
}

// If the type of the current token is TokenIf, consume the token and
// Ifs TRUE.
static int consumeIf() {
    if (globals.token->type == TokenIf) {
        globals.token = globals.token->next;
        return 1;
    }
    return 0;
}

// If the current token is the expected sign, consume the token. Otherwise
// reports an error.
static void expectSign(char *op) {
    if (globals.token->type == TokenReserved &&
            strlen(op) == (size_t)globals.token->len &&
            memcmp(globals.token->str, op, (size_t)globals.token->len) == 0) {
        globals.token = globals.token->next;
        return;
    }
    errorAt(globals.token->str, "'%s' is expected.", op);
}

// If the current token is the number token, consume the token and returns the
// value. Otherwise reports an error.
static int expectNumber() {
    if (globals.token->type == TokenNumber) {
        int val = globals.token->val;
        globals.token = globals.token->next;
        return val;
    }
    errorAt(globals.token->str, "Non number appears.");
    return 0;
}

static int atEOF() {
    return globals.token->type == TokenEOF;
}

static Node *newNode(NodeKind kind, Node *lhs, Node *rhs) {
    Node *n = calloc(1, sizeof(Node));
    n->kind = kind;
    n->lhs =  lhs;
    n->rhs =  rhs;
    return n;
}

static Node *newNodeNum(int val) {
    Node *n = newNode(NodeNum, NULL, NULL);
    n->val = val;
    return n;
}

static Node *newNodeLVar(int offset) {
    Node *n = newNode(NodeLVar, NULL, NULL);
    n->offset = offset;
    return n;
}

// Find local variable by name. Return LVar* when variable found. When not,
// returns NULL.
static LVar *findLVar(char *name, int len) {
    for (LVar *v = globals.locals; v != NULL; v = v->next) {
        if (v->len == len && memcmp(v->name, name, (size_t)len) == 0)
            return v;
    }
    return NULL;
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

static Node *stmt() {
    if (consumeReturn()) {
        Node *n = expr();
        expectSign(";");
        n = newNode(NodeReturn, n, NULL);
        return n;
    } else if (consumeIf()) {
        Node *cond;
        Node *body;
        expectSign("(");
        cond = expr();
        expectSign(")");
        body = stmt();
        return newNode(NodeIf, cond, body);
    } else {
        Node *n = expr();
        expectSign(";");
        return n;
    }
}

static Node *expr() {
    Node *n = assign();
    return n;
}

static Node *assign() {
    Node *n = equality();
    if (consumeToken("=")) {
        n = newNode(NodeAssign, n, assign());
    }
    return n;
}

static Node *equality() {
    Node *n = relational();
    for (;;) {
        if (consumeToken("==")) {
            n = newNode(NodeEq, n, relational());
        } else if (consumeToken("!=")) {
            n = newNode(NodeNeq, n, relational());
        } else {
            return n;
        }
    }
}

static Node *relational() {
    Node *n = add();
    for (;;) {
        if (consumeToken("<")) {
            n = newNode(NodeLT, n, add());
        } else if (consumeToken(">")) {
            n = newNode(NodeLT, add(), n);
        } else if (consumeToken("<=")) {
            n = newNode(NodeLE, n, add());
        } else if (consumeToken(">=")) {
            n = newNode(NodeLE, add(), n);
        } else {
            return n;
        }
    }
}

static Node *add() {
    Node *n = mul();
    for (;;) {
        if (consumeToken("+")) {
            n = newNode(NodeAdd, n, mul());
        } else if (consumeToken("-")) {
            n = newNode(NodeSub, n, mul());
        } else {
            return n;
        }
    }
}

static Node *mul() {
    Node *n = unary();
    for (;;) {
        if (consumeToken("*")) {
            n = newNode(NodeMul, n, unary());
        } else if (consumeToken("/")) {
            n = newNode(NodeDiv, n, unary());
        } else {
            return n;
        }
    }
}

static Node *unary() {
    if (consumeToken("+")) {
        return newNode(NodeAdd, newNodeNum(0), primary());
    } else if (consumeToken("-")) {
        return newNode(NodeSub, newNodeNum(0), primary());
    } else {
        return primary();
    }
}

static Node *primary() {
    if (consumeToken("(")) {
        Node *n = expr();
        expectSign(")");
        return n;
    }

    Token *t = consumeIdent();
    if (t) {
        LVar *lvar = findLVar(t->str, t->len);
        if (!lvar) {
            int lastoffset = 0;
            if (globals.locals)
                lastoffset = globals.locals->offset;

            lvar = (LVar*)calloc(1, sizeof(LVar));
            lvar->name = t->str;
            lvar->len = t->len;
            lvar->offset = lastoffset + 8;  // Use 8bytes per one variable.
            lvar->next = globals.locals;
            globals.locals = lvar;
        }
        return newNodeLVar(lvar->offset);
    }
    return newNodeNum(expectNumber());
}