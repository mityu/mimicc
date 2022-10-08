#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "mimic.h"

static Token *newToken(TokenType type, Token *current, char *str, int len);
static int atEOF();
static Node *function();
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

static void errorIdentExpected(char *at) {
    errorAt(at, "An identifier is expected");
}

static void errorUnexpectedEOF() {
    errorAt(globals.token->str, "Unexpected EOF");
}

static int isToken(char *p, char *op) {
    return hasPrefix(p, op) && !isAlnum(p[strlen(op)]);
}

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

        if (isToken(p, "int")) {
            current = newToken(TokenTypeName, current, p, 3);
            p += 3;
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
        if (strchr("+-*/%()=;<>{},&", *p)) {
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
static int consumeReserved(char *op) {
    if (atEOF()) {
        errorUnexpectedEOF();
        return 0;
    }
    if (globals.token->type == TokenReserved &&
            strlen(op) == (size_t)globals.token->len &&
            memcmp(globals.token->str, op, (size_t)globals.token->len) == 0) {
        globals.token = globals.token->next;
        return 1;
    }
    return 0;
}

// If the current token is TokenTypeName, consume the token and return the
// pointer to the token structure. Return NULL otherwise.
static Token *consumeTypeName() {
    if (atEOF()) {
        errorUnexpectedEOF();
        return NULL;
    }
    if (globals.token->type == TokenTypeName) {
        Token *t = globals.token;
        globals.token = globals.token->next;
        return t;
    }
    return NULL;
}

// If the type of the current token is TokenIdent, consume the token and
// returns the pointer to token structure.  If not, returns NULL instead.
static Token *consumeIdent() {
    if (atEOF()) {
        errorUnexpectedEOF();
        return NULL;
    }
    if (globals.token->type == TokenIdent) {
        Token *t = globals.token;
        globals.token = globals.token->next;
        return t;
    }
    return NULL;
}

// If the type of the current token is `type`, consume the token and returns
// TRUE.
static int consumeCertainTokenType(TokenType type) {
    if (atEOF()) {
        errorUnexpectedEOF();
        return 0;
    }
    if (globals.token->type == type) {
        globals.token = globals.token->next;
        return 1;
    }
    return 0;
}

// If the current token is the expected sign, consume the token. Otherwise
// reports an error.
static void expectSign(char *op) {
    if (atEOF()) {
        errorUnexpectedEOF();
        return;
    }
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
    if (atEOF()) {
        errorUnexpectedEOF();
        return 0;
    }
    if (globals.token->type == TokenNumber) {
        int val = globals.token->val;
        globals.token = globals.token->next;
        return val;
    }
    errorAt(globals.token->str, "Non number appears.");
    return 0;
}

static int assureSign(char *op) {
    if (atEOF()) {
        errorUnexpectedEOF();
        return 0;
    }
    if (globals.token->type == TokenReserved &&
            strlen(op) == (size_t)globals.token->len &&
            memcmp(globals.token->str, op, (size_t)globals.token->len) == 0) {
        return 1;
    }
    errorAt(globals.token->str, "'%s' is expected.", op);
    return 0;
}

static int atEOF() {
    return globals.token->type == TokenEOF;
}

static LVar *newLVar(Token *t, int offset) {
    LVar *v = (LVar *)calloc(1, sizeof(LVar));
    v->next = NULL;
    v->name = t->str;
    v->len = t->len;
    v->offset = offset;
    return v;
}

static Node *newNode(NodeKind kind) {
    Node *n = calloc(1, sizeof(Node));
    n->kind = kind;
    n->lhs  = NULL;
    n->rhs  = NULL;
    n->body = NULL;
    n->next = NULL;
    n->outerBlock = globals.currentBlock;
    return n;
}

static Node *newNodeBinary(NodeKind kind, Node *lhs, Node *rhs) {
    Node *n = newNode(kind);
    n->lhs = lhs;
    n->rhs = rhs;
    return n;
}

static Node *newNodeNum(int val) {
    Node *n = newNode(NodeNum);
    n->val = val;
    return n;
}

static Node *newNodeLVar(int offset) {
    Node *n = newNode(NodeLVar);
    n->offset = offset;
    return n;
}

static Node *newNodeFor() {
    Node *n = newNode(NodeFor);
    n->initializer = NULL;
    n->condition = NULL;
    n->iterator = NULL;
    n->blockID = globals.blockCount++;
    return n;
}

static Node *newNodeFCall() {
    Node *n = newNode(NodeFCall);
    n->fcall = (FCall*)calloc(1, sizeof(FCall));
    return n;
}

static Node *newNodeFunction() {
    Node *n = newNode(NodeFunction);
    n->func = (Function *)calloc(1, sizeof(Function));
    return n;
}

// Find local variable by name. Return LVar* when variable found. When not,
// returns NULL.
static LVar *findLVar(char *name, int len) {
    Node *block = globals.currentBlock;
    for (;;) {
        for (LVar *v = block->localVars; v; v = v->next) {
            if (v->len == len && memcmp(v->name, name, (size_t)len) == 0)
                return v;
        }
        block = block->outerBlock;
        if (!block)
            break;
    }
    for (LVar *v = globals.currentFunction->args; v; v = v->next) {
        if (v->len == len && memcmp(v->name, name, (size_t)len) == 0)
            return v;
    }
    return NULL;
}

void program() {
    Node body;
    Node *last = &body;
    body.next = NULL;
    globals.code = newNode(NodeBlock);
    while (!atEOF()) {
        last->next = function();
        last = last->next;
    }
    globals.code->body = body.next;
}

static Node *function() {
    Token *type = NULL;
    Token *ident = NULL;

    type = consumeTypeName();
    if (!type) {
        errorAt(globals.token->str, "An type is required.");
        return NULL;
    }

    ident = consumeIdent();
    if (ident) {
        Node *n = newNodeFunction();
        int argsCount = 0;
        LVar argHead;
        LVar *args = &argHead;

        // Dive into the new block.
        globals.currentBlock = n;
        globals.currentFunction = n->func;
        argHead.next = NULL;
        n->func->name = ident->str;
        n->func->len = ident->len;

        expectSign("(");
        if (!consumeReserved(")")) {
            // When argument exists.
            for (;;) {
                int offset = 0;
                Token *typeToken = NULL;
                Token *argToken = NULL;

                typeToken = consumeTypeName();
                if (!typeToken) {
                    errorAt(globals.token->str + globals.token->len,
                            "An type is expected");
                    return NULL;
                }

                argToken = consumeIdent();
                if (!argToken) {
                    errorIdentExpected(globals.token->str + globals.token->len);
                    return NULL;
                }
                argsCount++;
                // Arguments are all copied onto stack at the head of function.
                if (argsCount > REG_ARGS_MAX_COUNT) {
                    offset = -((argsCount - REG_ARGS_MAX_COUNT + 1) * 8);
                } else {
                    offset = argsCount * 8;
                }
                args->next = newLVar(argToken, offset);
                args = args->next;
                if (!consumeReserved(","))
                    break;
            }
            expectSign(")");
        }
        n->func->args = argHead.next;
        n->func->argsCount = argsCount;

        // Handle function body
        assureSign("{");
        n->body = stmt();

        // Escape from this block to the outer one.
        globals.currentBlock = n->outerBlock;
        return n;
    } else {
        errorIdentExpected(globals.token->str);
    }
    return NULL;
}

static Node *stmt() {
    Token *typeToken = consumeTypeName();
    if (typeToken) {
        Token *ident = consumeIdent();
        LVar *lvar = NULL;
        int varCount = 0;

        if (!ident) {
            errorIdentExpected(globals.token->str);
            return NULL;
        }
        expectSign(";");

        lvar = findLVar(ident->str, ident->len);
        if (lvar) {
            errorAt(ident->str, "Redefinition of variable");
            return NULL;
        }
        globals.currentBlock->localVarCount++;


        if (globals.currentFunction->argsCount > REG_ARGS_MAX_COUNT) {
            varCount =
                globals.currentFunction->argsCount - REG_ARGS_MAX_COUNT;
        }
        for (Node *block = globals.currentBlock; block; block = block->outerBlock) {
            varCount += block->localVarCount;
        }
        lvar = newLVar(ident, varCount * 8);

        // Register variable.
        if (globals.currentBlock->localVars) {
            lvar->next = globals.currentBlock->localVars;
            globals.currentBlock->localVars = lvar;
        } else {
            globals.currentBlock->localVars = lvar;
        }

        return newNodeLVar(lvar->offset);
    } else if (consumeReserved("{")) {
        Node *n = newNode(NodeBlock);
        Node body;
        Node *last = &body;
        body.next = NULL;
        globals.currentBlock = n;  // Dive into the new block.
        while (!consumeReserved("}")) {
            last->next = stmt();
            last = last->next;
        }
        n->body = body.next;
        globals.currentBlock = n->outerBlock;  // Escape from this block to the outer one.
        return n;
    } else if (consumeCertainTokenType(TokenReturn)) {
        Node *n = expr();
        expectSign(";");
        n = newNodeBinary(NodeReturn, n, NULL);
        return n;
    } else if (consumeCertainTokenType(TokenIf)) {
        Node *n = newNode(NodeIf);
        Node elseblock;
        Node *lastElse = &elseblock;
        elseblock.next = NULL;
        n->blockID = globals.blockCount++;

        expectSign("(");
        n->condition = expr();
        expectSign(")");
        n->body = stmt();

        while (consumeCertainTokenType(TokenElseif)) {
            Node *e = newNode(NodeElseif);
            expectSign("(");
            e->condition = expr();
            expectSign(")");
            e->body = stmt();
            lastElse->next = e;
            lastElse = lastElse->next;
        }

        if (consumeCertainTokenType(TokenElse)) {
            Node *e = newNode(NodeElse);
            e->body = stmt();
            lastElse->next = e;
            lastElse = lastElse->next;
        }
        n->elseblock = elseblock.next;

        return n;
    } else if (consumeCertainTokenType(TokenFor)) {
        Node *n = newNodeFor();
        expectSign("(");
        if (!consumeReserved(";")) {
            n->initializer = expr();
            expectSign(";");
        }
        if (!consumeReserved(";")) {
            n->condition = expr();
            expectSign(";");
        }
        if (!consumeReserved(")")) {
            n->iterator = expr();
            expectSign(")");
        }
        n->body = stmt();
        return n;
    } else if (consumeCertainTokenType(TokenWhile)) {
        Node *n = newNodeFor();
        expectSign("(");
        n->condition = expr();
        expectSign(")");
        n->body = stmt();
        return n;
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
    if (consumeReserved("=")) {
        n = newNodeBinary(NodeAssign, n, assign());
    }
    return n;
}

static Node *equality() {
    Node *n = relational();
    for (;;) {
        if (consumeReserved("==")) {
            n = newNodeBinary(NodeEq, n, relational());
        } else if (consumeReserved("!=")) {
            n = newNodeBinary(NodeNeq, n, relational());
        } else {
            return n;
        }
    }
}

static Node *relational() {
    Node *n = add();
    for (;;) {
        if (consumeReserved("<")) {
            n = newNodeBinary(NodeLT, n, add());
        } else if (consumeReserved(">")) {
            n = newNodeBinary(NodeLT, add(), n);
        } else if (consumeReserved("<=")) {
            n = newNodeBinary(NodeLE, n, add());
        } else if (consumeReserved(">=")) {
            n = newNodeBinary(NodeLE, add(), n);
        } else {
            return n;
        }
    }
}

static Node *add() {
    Node *n = mul();
    for (;;) {
        if (consumeReserved("+")) {
            n = newNodeBinary(NodeAdd, n, mul());
        } else if (consumeReserved("-")) {
            n = newNodeBinary(NodeSub, n, mul());
        } else {
            return n;
        }
    }
}

static Node *mul() {
    Node *n = unary();
    for (;;) {
        if (consumeReserved("*")) {
            n = newNodeBinary(NodeMul, n, unary());
        } else if (consumeReserved("/")) {
            n = newNodeBinary(NodeDiv, n, unary());
        } else if (consumeReserved("%")) {
            n = newNodeBinary(NodeDivRem, n, unary());
        } else {
            return n;
        }
    }
}

static Node *unary() {
    if (consumeReserved("+")) {
        return newNodeBinary(NodeAdd, newNodeNum(0), primary());
    } else if (consumeReserved("-")) {
        return newNodeBinary(NodeSub, newNodeNum(0), primary());
    } else if (consumeReserved("&")) {
        return newNodeBinary(NodeAddress, NULL, unary());
    } else if (consumeReserved("*")) {
        return newNodeBinary(NodeDeref, NULL, unary());
    } else {
        return primary();
    }
}

static Node *primary() {
    Token *ident = NULL;
    Token *type = NULL;
    if (consumeReserved("(")) {
        Node *n = expr();
        expectSign(")");
        return n;
    }

    ident = consumeIdent();
    if (ident && consumeReserved("(")) { // Function call.
        Node *n = newNodeFCall();
        n->fcall->name = ident->str;
        n->fcall->len = ident->len;
        if (consumeReserved(")")) {
            return n;
        }
        Node *arg;
        for (;;) {
            arg = expr();
            arg->next = n->fcall->args;
            n->fcall->args = arg;
            ++n->fcall->argsCount;
            if (!consumeReserved(","))
                break;
        }
        expectSign(")");
        return n;
    } else if (ident) { // Use of variables.
        LVar *lvar = findLVar(ident->str, ident->len);
        if (!lvar) {
            errorAt(ident->str, "Undefined variable");
            return NULL;
        }
        return newNodeLVar(lvar->offset);
    }

    return newNodeNum(expectNumber());
}
