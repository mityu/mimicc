#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "mimic.h"

static Token *newToken(TokenType type, Token *current, char *str, int len);
static int atEOF();
static TypeInfo *parseType();
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

static void errorIdentExpected() {
    errorAt(globals.token->str, "An identifier is expected");
}

static void errorTypeExpected() {
    errorAt(globals.token->str, "An type is expected");
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
    t->prev = current;
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
            current->varType = TypeInt;
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

        if (isToken(p, "sizeof")) {
            current = newToken(TokenSizeof, current, p, 6);
            p += 6;
            continue;
        }

        if (hasPrefix(p, "==") || hasPrefix(p, "!=") ||
                hasPrefix(p, ">=") || hasPrefix(p, "<=")  ||
                hasPrefix(p, "++") || hasPrefix(p, "--")) {
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

static LVar *newLVar(Token *t, TypeInfo *typeInfo, int offset) {
    LVar *v = (LVar *)calloc(1, sizeof(LVar));
    v->next = NULL;
    v->name = t->str;
    v->len = t->len;
    v->type = typeInfo;
    v->offset = offset;
    return v;
}

struct Function *newFunction() {
    Function *f = (Function*)calloc(1, sizeof(Function));
    return f;
};

static Node *newNode(NodeKind kind, TypeInfo *type) {
    Node *n = calloc(1, sizeof(Node));
    n->kind = kind;
    n->lhs  = NULL;
    n->rhs  = NULL;
    n->body = NULL;
    n->next = NULL;
    n->outerBlock = globals.currentBlock;
    n->token = globals.token->prev;
    n->type = type;
    return n;
}

static Node *newNodeBinary(NodeKind kind, Node *lhs, Node *rhs, TypeInfo *type) {
    Node *n = newNode(kind, type);
    n->lhs = lhs;
    n->rhs = rhs;
    return n;
}

static Node *newNodeNum(int val) {
    Node *n = newNode(NodeNum, &Types.Number);
    n->val = val;
    return n;
}

static Node *newNodeLVar(int offset, TypeInfo *type) {
    Node *n = newNode(NodeLVar, type);
    n->offset = offset;
    return n;
}

static Node *newNodeFor() {
    Node *n = newNode(NodeFor, &Types.None);
    n->initializer = NULL;
    n->condition = NULL;
    n->iterator = NULL;
    n->blockID = globals.blockCount++;
    return n;
}

static Node *newNodeFCall(TypeInfo *retType) {
    Node *n = newNode(NodeFCall, retType);
    n->fcall = (FCall*)calloc(1, sizeof(FCall));
    return n;
}

static Node *newNodeFunction() {
    Node *n = newNode(NodeFunction, &Types.None);
    n->func = (Function *)calloc(1, sizeof(Function));
    return n;
}

// Find local variable in current block by name. Return LVar* when variable
// found. When not, returns NULL.
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

Function *findFunction(char *name, int len) {
    for (Function *f = globals.functions; f; f = f->next) {
        if (f->len == len && memcmp(f->name, name, (size_t)len) == 0)
            return f;
    }
    return NULL;
}

static TypeInfo *newTypeInfo(TypeKind kind) {
    TypeInfo *t = (TypeInfo *)calloc(1, sizeof(TypeInfo));
    t->type = kind;
    return t;
}

// Parse declared type and return type information.  Return NULL if type
// doesn't appear.
static TypeInfo *parseType() {
    TypeInfo *typeInfo = NULL;
    Token *type = consumeTypeName();
    if (!type) {
        return NULL;
    }
    typeInfo = newTypeInfo(type->varType);
    while (consumeReserved("*")) {
        TypeInfo *t = newTypeInfo(TypePointer);
        t->ptrTo = typeInfo;
        typeInfo = t;
    }
    return typeInfo;
}

static int sizeOf(Node *n) {
    TypeKind type = n->type->type;
    if (type == TypeInt || type == TypeNumber) {
        return 4;
    } else if (type == TypePointer) {
        return 8;
    }
    errorUnreachable();
    return -1;
}

void program() {
    Node body;
    Node *last = &body;
    body.next = NULL;
    globals.code = newNode(NodeBlock, &Types.None);
    while (!atEOF()) {
        Node *n = function();
        if (n) {
            last->next = n;
            last = last->next;
        }
    }
    globals.code->body = body.next;
}

static Node *function() {
    TypeInfo *type = NULL;
    Token *ident = NULL;

    type = parseType();
    if (!type) {
        errorTypeExpected();
        return NULL;
    }

    ident = consumeIdent();
    if (ident) {
        Node *n = NULL;
        Function *f = newFunction();
        Function *funcFound = NULL;
        int argsCount = 0;
        int argNum = 0;
        int justDeclaring = 0;
        LVar argHead;
        LVar *args = &argHead;

        argHead.next = NULL;
        f->name = ident->str;
        f->len = ident->len;
        f->retType = type;

        expectSign("(");
        if (!consumeReserved(")")) {
            // When argument exists.
            for (;;) {
                TypeInfo *typeInfo = NULL;
                Token *argToken = NULL;

                typeInfo = parseType();
                if (!typeInfo) {
                    errorTypeExpected();
                    return NULL;
                }

                argToken = consumeIdent();
                if (!argToken) {
                    // TODO: Allow omitting variable name when just declaring
                    // function.
                    errorIdentExpected();
                    return NULL;
                }
                argsCount++;
                // Temporally, offset is 0 here.  It's computed later if
                // needed.
                args->next = newLVar(argToken, typeInfo, 0);
                args = args->next;
                if (!consumeReserved(","))
                    break;
            }
            expectSign(")");
        }
        f->args = argHead.next;
        f->argsCount = argsCount;

        if (consumeReserved(";")) {
            justDeclaring = 1;
        }

        funcFound = findFunction(f->name, f->len);
        if (funcFound) {
            if (!justDeclaring) {
                if (funcFound->haveImpl) {
                    errorAt(ident->str, "Function is defined twice");
                } else {
                    funcFound->haveImpl = 1;
                }
            }
        } else {
            // Register function.
            f->next = globals.functions;
            globals.functions = f;
        }

        if (justDeclaring) {
            return NULL;
        }


        n = newNodeFunction();
        n->func = f;
        n->token = ident;

        // Dive into the new block.
        globals.currentBlock = n;
        globals.currentFunction = n->func;

        // Compute argument variables' offset.
        // Note that arguments are all copied onto stack at the head of
        // function.
        for (LVar *v = f->args; v; v = v->next) {
            ++argNum;
            if (argNum > REG_ARGS_MAX_COUNT) {
                v->offset = -((argNum - REG_ARGS_MAX_COUNT + 1) * 8);
            } else {
                v->offset = argNum * 8;
            }
        }

        // Handle function body
        assureSign("{");
        n->body = stmt();

        // Escape from this block to the outer one.
        globals.currentBlock = n->outerBlock;
        return n;
    } else {
        errorIdentExpected();
    }
    return NULL;
}

static Node *stmt() {
    TypeInfo *typeInfo = parseType();
    if (typeInfo) {
        Token *ident = consumeIdent();
        LVar *lvar = NULL;
        int varCount = 0;

        if (!ident) {
            errorIdentExpected();
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
        lvar = newLVar(ident, typeInfo, varCount * 8);

        // Register variable.
        if (globals.currentBlock->localVars) {
            lvar->next = globals.currentBlock->localVars;
            globals.currentBlock->localVars = lvar;
        } else {
            globals.currentBlock->localVars = lvar;
        }

        return newNodeLVar(lvar->offset, lvar->type);
    } else if (consumeReserved("{")) {
        Node *n = newNode(NodeBlock, &Types.None);
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
        n = newNodeBinary(NodeReturn, n, NULL, n->type);
        n->token = n->token->prev;
        return n;
    } else if (consumeCertainTokenType(TokenIf)) {
        Node *n = newNode(NodeIf, &Types.None);
        Node elseblock;
        Node *lastElse = &elseblock;
        elseblock.next = NULL;
        n->blockID = globals.blockCount++;

        expectSign("(");
        n->condition = expr();
        expectSign(")");
        n->body = stmt();

        while (consumeCertainTokenType(TokenElseif)) {
            Node *e = newNode(NodeElseif, &Types.None);
            expectSign("(");
            e->condition = expr();
            expectSign(")");
            e->body = stmt();
            lastElse->next = e;
            lastElse = lastElse->next;
        }

        if (consumeCertainTokenType(TokenElse)) {
            Node *e = newNode(NodeElse, &Types.None);
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
    Token *t = globals.token;
    if (consumeReserved("=")) {
        n = newNodeBinary(NodeAssign, n, assign(), n->type);
        n->token = t;
    }
    return n;
}

static Node *equality() {
    Node *n = relational();
    for (;;) {
        Token *t = globals.token;
        if (consumeReserved("==")) {
            n = newNodeBinary(NodeEq, n, relational(), &Types.Number);
        } else if (consumeReserved("!=")) {
            n = newNodeBinary(NodeNeq, n, relational(), &Types.Number);
        } else {
            return n;
        }
        n->token = t;
    }
}

static Node *relational() {
    Node *n = add();
    for (;;) {
        Token *t = globals.token;
        if (consumeReserved("<")) {
            n = newNodeBinary(NodeLT, n, add(), &Types.Number);
        } else if (consumeReserved(">")) {
            n = newNodeBinary(NodeLT, add(), n, &Types.Number);
        } else if (consumeReserved("<=")) {
            n = newNodeBinary(NodeLE, n, add(), &Types.Number);
        } else if (consumeReserved(">=")) {
            n = newNodeBinary(NodeLE, add(), n, &Types.Number);
        } else {
            return n;
        }
        n->token = t;
    }
}

static Node *add() {
    Node *n = mul();
    for (;;) {
        Token *t = globals.token;
        if (consumeReserved("+")) {
            n = newNodeBinary(NodeAdd, n, mul(), n->type);
        } else if (consumeReserved("-")) {
            n = newNodeBinary(NodeSub, n, mul(), n->type);
        } else {
            return n;
        }
        n->token = t;
    }
}

static Node *mul() {
    Node *n = unary();
    for (;;) {
        Token *t = globals.token;
        if (consumeReserved("*")) {
            n = newNodeBinary(NodeMul, n, unary(), n->type);
        } else if (consumeReserved("/")) {
            n = newNodeBinary(NodeDiv, n, unary(), n->type);
        } else if (consumeReserved("%")) {
            n = newNodeBinary(NodeDivRem, n, unary(), n->type);
        } else {
            return n;
        }
        n->token = t;
    }
}

static Node *unary() {
    Token *tokenOperator = globals.token;
    Node *n = NULL;
    if (consumeReserved("+")) {
        Node *rhs = primary();
        n = newNodeBinary(NodeAdd, newNodeNum(0), rhs, rhs->type);
    } else if (consumeReserved("-")) {
        Node *rhs = primary();
        n = newNodeBinary(NodeSub, newNodeNum(0), rhs, rhs->type);
    } else if (consumeReserved("&")) {
        Node *rhs = unary();
        TypeInfo *type = newTypeInfo(TypePointer);
        type->ptrTo = rhs->type;
        n = newNodeBinary(NodeAddress, NULL, rhs, type);
    } else if (consumeReserved("*")) {
        Node *rhs = unary();
        if (!(rhs->type && rhs->type->type == TypePointer)) {
            errorAt(rhs->token->str,
                    "Cannot dereference a non-pointer value.");
        }
        n = newNodeBinary(NodeDeref, NULL, rhs, rhs->type->ptrTo);
    } else if (consumeReserved("++")) {
        Node *rhs = unary();
        n = newNodeBinary(NodePreIncl, NULL, rhs, rhs->type);
    } else if (consumeReserved("--")) {
        Node *rhs = unary();
        n = newNodeBinary(NodePreDecl, NULL, rhs, rhs->type);
    } else if (consumeCertainTokenType(TokenSizeof)) {
        Node *rhs = unary();
        int size = sizeOf(rhs);
        n = newNodeNum(size);
    } else {
        return primary();
    }
    n->token = tokenOperator;
    return n;
}

static Node *primary() {
    Node *n = NULL;
    Token *ident = NULL;
    Token *type = NULL;

    ident = consumeIdent();
    if (ident) {
        if (consumeReserved("(")) { // Function call.
            Node *arg;
            Function *f;

            f = findFunction(ident->str, ident->len);
            if (!f) {
                errorAt(ident->str, "No such function.");
            }

            n = newNodeFCall(f->retType);
            n->fcall->name = ident->str;
            n->fcall->len = ident->len;
            n->token = ident;

            if (consumeReserved(")")) {
                return n;
            }
            for (;;) {
                arg = expr();
                arg->next = n->fcall->args;
                n->fcall->args = arg;
                ++n->fcall->argsCount;
                if (!consumeReserved(","))
                    break;
            }
            expectSign(")");
        } else { // Use of variables or postfix increment/decrement.
            LVar *lvar = findLVar(ident->str, ident->len);
            if (!lvar) {
                errorAt(ident->str, "Undefined variable");
                return NULL;
            }
            n = newNodeLVar(lvar->offset, lvar->type);
            n->type = lvar->type;
        }
    } else if (consumeReserved("(")) {
        n = expr();
        expectSign(")");
    } else {
        n = newNodeNum(expectNumber());
    }

    if (consumeReserved("++")) {
        n = newNodeBinary(NodePostIncl, n, NULL, n->type);
    } else if (consumeReserved("--")) {
        n = newNodeBinary(NodePostDecl, n, NULL, n->type);
    }

    return n;
}
