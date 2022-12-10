#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "mimic.h"

static Token *newToken(TokenType type, Token *current, char *str, int len);
static int atEOF();
static TypeInfo *parseBaseType();
static TypeInfo *parsePointerType(TypeInfo *baseType);
static Node *decl();
static Node *stmt();
static Node *vardecl();
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

        if (hasPrefix(p, "==") || hasPrefix(p, "!=") ||
                hasPrefix(p, ">=") || hasPrefix(p, "<=")  ||
                hasPrefix(p, "+=") || hasPrefix(p, "-=") ||
                hasPrefix(p, "++") || hasPrefix(p, "--")) {
            current = newToken(TokenReserved, current, p, 2);
            p += 2;
            continue;
        }
        if (strchr("+-*/%()=;[]<>{},&", *p)) {
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

// Find global variable by name. Return LVar* when variable is found. Returns
// NULL when not.
LVar *findGlobalVar(char *name, int len) {
    for (LVar *v = globals.vars; v; v = v->next) {
        if (v->len == len && memcmp(v->name, name, (size_t)len) == 0)
            return v;
    }
    return NULL;
}

// Find local variable in current block by name. Return LVar* when variable
// found. When not, returns NULL.
// Note that this function does NOT search global variables.
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

// Parse base of declared type and return its information.  Return NULL if type
// doesn't appear.
static TypeInfo *parseBaseType() {
    Token *type = consumeTypeName();
    if (!type) {
        return NULL;
    }
    return newTypeInfo(type->varType);
}

// Parse pointer declarations.
static TypeInfo *parsePointerType(TypeInfo *baseType) {
    TypeInfo *typeInfo = baseType;
    while (consumeReserved("*")) {
        TypeInfo *t = newTypeInfo(TypePointer);
        t->baseType = typeInfo;
        typeInfo = t;
    }
    return typeInfo;
}

// Return size of given type.  If computing failed, exit program.
int sizeOf(TypeInfo *ti) {
    if (ti->type == TypeInt || ti->type == TypeNumber) {
        return 4;
    } else if (ti->type == TypePointer) {
        return 8;
    } else if (ti->type == TypeArray) {
        return sizeOf(ti->baseType) * ti->arraySize;
    }
    errorUnreachable();
    return -1;
}

// Determine the type of expression result for arithmetic operands.
// For the details, see the "6.3.1 Arithmetic operands" chapter (P.68) in this
// C11 draft PDF:
// https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1548.pdf
static TypeInfo *getTypeForArithmeticOperands(TypeInfo *lhs, TypeInfo *rhs) {
    if (lhs->type == rhs->type) {
        return lhs;
    } else if (lhs->type == TypeNumber) {
        return rhs;
    } else if (rhs->type == TypeNumber) {
        return lhs;
    }
    return NULL;
}

void program() {
    Node body;
    Node *last = &body;
    body.next = NULL;
    globals.code = newNode(NodeBlock, &Types.None);
    while (!atEOF()) {
        Node *n = decl();
        if (n) {
            last->next = n;
            last = last->next;
        }
    }
    globals.code->body = body.next;
}

static Node *decl() {
    TypeInfo *type = NULL;
    Token *ident = NULL;

    type = parseBaseType();
    if (!type) {
        errorTypeExpected();
        return NULL;
    }
    type = parsePointerType(type);

    ident = consumeIdent();
    if (ident) {
        Node *n = NULL;
        Function *f = NULL;
        Function *funcFound = NULL;
        int argsCount = 0;
        int argNum = 0;
        int argOffset = 0;
        int justDeclaring = 0;
        LVar argHead;
        LVar *args = &argHead;
        Token *missingIdentOfArg = NULL;

        if (!consumeReserved("(")) {
            // Opening bracket isn't found. Must be global variable
            // declaration.
            LVar *gvar = NULL;

            if (findGlobalVar(ident->str, ident->len)) {
                errorAt(ident->str, "Redefinition of variable.");
                return NULL;
            }

            if (consumeReserved("[")) { // Array declaration.
                TypeInfo *elemType = type;
                int arraySize = expectNumber();
                expectSign("]");
                type = newTypeInfo(TypeArray);
                type->arraySize = arraySize;
                type->baseType = elemType;
            }

            expectSign(";");

            // Global variable doesn't have an offset.
            gvar = newLVar(ident, type, -1);

            // Register variable.
            if (globals.vars) {
                gvar->next = globals.vars;
                globals.vars = gvar;
            } else {
                globals.vars = gvar;
            }
            return NULL;
        }

        argHead.next = NULL;
        f = newFunction();
        f->name = ident->str;
        f->len = ident->len;
        f->retType = type;


        if (!consumeReserved(")")) {
            // ")" doesn't follows just after "(", arguments must exist.
            // Parse arguments in this loop.
            for (;;) {
                TypeInfo *typeInfo = NULL;
                Token *argToken = NULL;

                typeInfo = parseBaseType();
                if (!typeInfo) {
                    errorTypeExpected();
                    return NULL;
                }
                typeInfo = parsePointerType(typeInfo);

                argToken = consumeIdent();
                if (!argToken) {
                    // Argumnent name is omitted.  If omitting is OK is checked
                    // later.  Just do remember where argument name is missing
                    // here.
                    if (!missingIdentOfArg) {
                        // Remember the first argument whose name is omitted.
                        // Don't overwrite if any arguments' name is omitted
                        // before.
                        missingIdentOfArg = globals.token;
                    }
                    argToken = globals.token;
                }
                argsCount++;
                // Temporally, set offset to 0 here.  It's computed later if
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

        // Missing variable name although this is not just a function
        // declaration.
        if (!justDeclaring && missingIdentOfArg) {
            Token *token_save = globals.token;
            globals.token = missingIdentOfArg;
            errorIdentExpected();
            globals.token = token_save;
            return NULL;
        }

        funcFound = findFunction(f->name, f->len);
        if (funcFound) {
            if (!justDeclaring) {
                if (funcFound->haveImpl) {
                    errorAt(ident->str, "Function is defined twice");
                } else {
                    // Argument name may be omitted with function declaration.
                    // Function implementation must have argument name, so
                    // replace it to make sure argument name can be found.
                    funcFound->args = f->args;
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
        argOffset = 0;
        argNum = 0;
        for (LVar *v = f->args; v; v = v->next) {
            ++argNum;
            if (argNum > REG_ARGS_MAX_COUNT) {
                argOffset -= ONE_WORD_BYTES;
                v->offset = argOffset;
            } else {
                argOffset += sizeOf(v->type);
                v->offset = argOffset;
                if (argNum == REG_ARGS_MAX_COUNT) {
                    argOffset = -ONE_WORD_BYTES;
                }
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
    Node *varDeclNode = vardecl();
    if (varDeclNode)
        return varDeclNode;

    if (consumeReserved("{")) {
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

// Parse variable declaration. If there's no variable declaration, returns
// NULL.
static Node *vardecl(){
    Node headNode;
    Node *n = &headNode;
    Token *ident = NULL;
    LVar *lvar = NULL;
    int totalVarSize = 0;
    int currentVarSize = 0;
    TypeInfo *baseType = NULL;
    TypeInfo *varType = NULL;

    baseType = parseBaseType();
    if (!baseType)
        return NULL;

    for (Node *block = globals.currentBlock; block; block = block->outerBlock) {
        totalVarSize += block->localVarSize;
    }


    for (;;) {
        varType = parsePointerType(baseType);
        ident = consumeIdent();
        if (!ident) {
            errorIdentExpected();
            return NULL;
        }

        if (consumeReserved("[")) {
            TypeInfo *elemType = varType;
            int arraySize = expectNumber();
            expectSign("]");
            varType = newTypeInfo(TypeArray);
            varType->arraySize = arraySize;
            varType->baseType = elemType;
        }

        lvar = findLVar(ident->str, ident->len);
        if (lvar) {
            errorAt(ident->str, "Redefinition of variable");
            return NULL;
        }

        currentVarSize = sizeOf(varType);
        totalVarSize += currentVarSize;
        globals.currentBlock->localVarSize += currentVarSize;

        lvar = newLVar(ident, varType, totalVarSize);

        // Register variable.
        if (globals.currentBlock->localVars) {
            lvar->next = globals.currentBlock->localVars;
            globals.currentBlock->localVars = lvar;
        } else {
            globals.currentBlock->localVars = lvar;
        }

        n->next = newNodeLVar(lvar->offset, lvar->type);
        n = n->next;

        if (!consumeReserved(","))
            break;
    }

    expectSign(";");

    return headNode.next;
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
    } else if (consumeReserved("+=")) {
        Node *lhs = n;
        Node *rhs = assign();
        TypeInfo *type = getTypeForArithmeticOperands(n->type, rhs->type);
        n = newNodeBinary(NodeAdd, lhs, rhs, type);
        n->token = t;
        n = newNodeBinary(NodeAssign, lhs, n, lhs->type);
        n->token = t;
    } else if (consumeReserved("-=")) {
        Node *lhs = n;
        Node *rhs = assign();
        TypeInfo *type = getTypeForArithmeticOperands(n->type, rhs->type);
        n = newNodeBinary(NodeSub, lhs, rhs, type);
        n->token = t;
        n = newNodeBinary(NodeAssign, lhs, n, lhs->type);
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
            Node *rhs = mul();
            TypeInfo *type = getTypeForArithmeticOperands(n->type, rhs->type);
            n = newNodeBinary(NodeAdd, n, rhs, type);
        } else if (consumeReserved("-")) {
            Node *rhs = mul();
            TypeInfo *type = getTypeForArithmeticOperands(n->type, rhs->type);
            n = newNodeBinary(NodeSub, n, rhs, type);
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
            Node *rhs = unary();
            TypeInfo *type = getTypeForArithmeticOperands(n->type, rhs->type);
            n = newNodeBinary(NodeMul, n, rhs, type);
        } else if (consumeReserved("/")) {
            Node *rhs = unary();
            TypeInfo *type = getTypeForArithmeticOperands(n->type, rhs->type);
            n = newNodeBinary(NodeDiv, n, rhs, type);
        } else if (consumeReserved("%")) {
            Node *rhs = unary();
            TypeInfo *type = getTypeForArithmeticOperands(n->type, rhs->type);
            n = newNodeBinary(NodeDivRem, n, rhs, type);
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
        type->baseType = rhs->type;
        n = newNodeBinary(NodeAddress, NULL, rhs, type);
    } else if (consumeReserved("*")) {
        Node *rhs = unary();
        TypeKind typeKind = rhs->type->type;
        if (!(rhs->type && (typeKind == TypePointer || typeKind == TypeArray))) {
            errorAt(tokenOperator->str,
                    "Cannot dereference a non-pointer value.");
        }

        n = newNodeBinary(NodeDeref, NULL, rhs, rhs->type->baseType);
    } else if (consumeReserved("++")) {
        Node *rhs = unary();
        n = newNodeBinary(NodePreIncl, NULL, rhs, rhs->type);
    } else if (consumeReserved("--")) {
        Node *rhs = unary();
        n = newNodeBinary(NodePreDecl, NULL, rhs, rhs->type);
    } else if (consumeCertainTokenType(TokenSizeof)) {
        Token *token_save = globals.token;
        TypeInfo *type = NULL;

        // First, do check for "sizeof(type)".
        if (consumeReserved("(")) {
            type = parseBaseType();
            if (type) {
                type = parsePointerType(type);
                expectSign(")");
            }
        }

        // Then, if "sizeof(type)" didn't match the case, do check for
        // "sizeof {unary-expr}".
        if (!type) {
            globals.token = token_save;
            type = unary()->type;
        }

        n = newNodeNum(sizeOf(type));
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
        } else { // Variable accessing.
            LVar *lvar = findLVar(ident->str, ident->len);
            LVar *gvar = NULL;
            if (lvar) {
                n = newNodeLVar(lvar->offset, lvar->type);
            } else if ((gvar = findGlobalVar(ident->str, ident->len)) != NULL) {
                n = newNode(NodeGVar, gvar->type);
            } else {
                errorAt(ident->str, "Undefined variable");
                return NULL;
            }
            n->token = ident;
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
    } else if (consumeReserved("[")) {
        Node *e = expr();
        expectSign("]");
        n = newNodeBinary(NodeAdd, n, e, n->type);
        n = newNodeBinary(NodeDeref, NULL, n, n->type->baseType);
    }

    return n;
}
