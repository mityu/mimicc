#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mimic.h"

// TODO: Accept using pointer to unknown type in struct member: e.g.:
//   struct A { struct B *obj };

#define abortIfEOF()  do { if (atEOF()) errorUnexpectedEOF(); } while (0)

static Token *newToken(TokenType type, Token *current, char *str, int len);
static int atEOF(void);
static Obj *parseEntireDeclaration(int allowTentativeArray);
static TypeInfo *parseBaseType(void);
static Obj *parseAdvancedTypeDeclaration(TypeInfo *baseType, int allowTentativeArray);
static Function *parseFuncArgDeclaration(void);
static int alignOf(const TypeInfo *ti);
static Node *decl(void);
static Node *stmt(void);
static Struct *structDeclaration(void);
static void structBody(Struct *s);
static Node *varDeclaration(void);
static Node *buildArrayInitNodes(Node *varNode, TypeInfo *varType, Node *initializer);
static Node *varInitializer(void);
static Node *varInitializerList(void);
static Node *expr(void);
static Node *assign(void);
static Node *logicalOR(void);
static Node *logicalAND(void);
static Node *equality(void);
static Node *relational(void);
static Node *add(void);
static Node *mul(void);
static Node *unary(void);
static Node *postfix(void);
static FCall *funcArgList(void);
static Node *primary(void);


_Noreturn static void errorIdentExpected(void) {
    errorAt(globals.token->str, "An identifier is expected");
}

_Noreturn static void errorTypeExpected(void) {
    errorAt(globals.token->str, "An type is expected");
}

_Noreturn static void errorUnexpectedEOF(void) {
    errorAt(globals.token->str, "Unexpected EOF");
}

// Check if current token matches `op` with type TokenReserved, and returns
// TRUE if so.
static int matchReserved(char *op) {
    abortIfEOF();
    if (globals.token->type == TokenReserved &&
            strlen(op) == (size_t)globals.token->len &&
            memcmp(globals.token->str, op, (size_t)globals.token->len) == 0) {
        return 1;
    }
    return 0;
}

// Check if type of current token equals to given type.  Returns TRUE if so.
static int matchCertainTokenType(TokenType type) {
    abortIfEOF();
    if (globals.token->type == type)
        return 1;
    return 0;
}

// If the current token is the expected token, consume the token and returns
// TRUE.
static int consumeReserved(char *op) {
    abortIfEOF();
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
static Token *consumeTypeName(void) {
    abortIfEOF();
    if (globals.token->type == TokenTypeName) {
        Token *t = globals.token;
        globals.token = globals.token->next;
        return t;
    }
    return NULL;
}

// If the type of the current token is TokenIdent, consume the token and
// returns the pointer to token structure.  If not, returns NULL instead.
static Token *consumeIdent(void) {
    abortIfEOF();
    if (globals.token->type == TokenIdent) {
        Token *t = globals.token;
        globals.token = globals.token->next;
        return t;
    }
    return NULL;
}

// If the type of the current token is TokenLiteralString, consume the token
// and returns the pointer to the token structure.  If not, returns NULL
// instead.
static Token *consumeLiteralString(void) {
    abortIfEOF();
    if (globals.token->type == TokenLiteralString) {
        Token *t = globals.token;
        globals.token = globals.token->next;
        return t;
    }
    return NULL;
}

static int consumeNumber(int *val) {
    abortIfEOF();

    if (globals.token->type == TokenNumber) {
        *val = globals.token->val;
        globals.token = globals.token->next;
        return 1;
    }
    return 0;
}

// If the type of the current token is `type`, consume the token and returns
// TRUE.
static int consumeCertainTokenType(TokenType type) {
    abortIfEOF();
    if (globals.token->type == type) {
        globals.token = globals.token->next;
        return 1;
    }
    return 0;
}

// If the current token is the expected sign, consume the token. Otherwise
// reports an error.
static void expectReserved(char *op) {
    if (matchReserved(op)) {
        globals.token = globals.token->next;
        return;
    }
    errorAt(globals.token->str, "'%s' is expected.", op);
}

// If the current token is the number token, consume the token and returns the
// value. Otherwise reports an error.
static int expectNumber(void) {
    abortIfEOF();
    if (globals.token->type == TokenNumber) {
        int val = globals.token->val;
        globals.token = globals.token->next;
        return val;
    }
    errorAt(globals.token->str, "Non number appears.");
}

// If the current token is ident token, consume the token and returns it.
// Otherwise exit anyway.
static Token *expectIdent(void) {
    Token *token = consumeIdent();
    if (token)
        return token;
    errorIdentExpected();
}

// Check if current token matches to the given token.  If so, do nothing, and
// otherwise reports an error and exits program.  Note that this function does
// not consume any tokens.
static void assureReserved(char *op) {
    if (matchReserved(op))
            return;
    errorAt(globals.token->str, "'%s' is expected.", op);
}

static int atEOF(void) {
    return globals.token->type == TokenEOF;
}

static Obj *newObj(Token *t, TypeInfo *typeInfo, int offset) {
    Obj *v = (Obj *)safeAlloc(sizeof(Obj));
    v->next = NULL;
    v->token = t;
    v->type = typeInfo;
    v->offset = offset;
    return v;
}

static Obj *newObjFunction(Token *t) {
    Obj *obj = newObj(t, NULL, 0);
    obj->func = (Function *)safeAlloc(sizeof(Function));
    return obj;
}

static Struct *newStruct(void) {
    Struct *s = (Struct *)safeAlloc(sizeof(Struct));
    s->totalSize = -1;
    return s;
}

// Generate new node object and returns it.  Members of kind, type, outerBlock,
// and token are automatically set to valid value.
static Node *newNode(NodeKind kind, TypeInfo *type) {
    Node *n = safeAlloc(sizeof(Node));
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

static Node *newNodeFor(void) {
    Node *n = newNode(NodeFor, &Types.None);
    n->initializer = NULL;
    n->condition = NULL;
    n->iterator = NULL;
    n->blockID = globals.blockCount++;
    return n;
}

static Node *newNodeFCall(TypeInfo *retType) {
    Node *n = newNode(NodeFCall, retType);
    n->fcall = (FCall*)safeAlloc(sizeof(FCall));
    return n;
}

static Node *newNodeFunction(Token *t) {
    Node *n = newNode(NodeFunction, &Types.None);
    n->func = newObjFunction(t);
    n->token = t;
    return n;
}

// Find global variable by name. Return LVar* when variable is found. Returns
// NULL when not.
static Obj *findGlobalVar(char *name, int len) {
    for (Obj *v = globals.vars; v; v = v->next) {
        if (v->token->len == len && memcmp(v->token->str, name, (size_t)len) == 0)
            return v;
    }
    return NULL;
}

// Find local variable in current block by name. Return LVar* when variable
// found. When not, returns NULL.
// Note that this function does NOT search global variables.
static Obj *findLVar(char *name, int len) {
    Node *block = globals.currentBlock;
    for (;;) {
        for (Obj *v = block->localVars; v; v = v->next) {
            if (v->token->len == len && memcmp(v->token->str, name, (size_t)len) == 0)
                return v;
        }
        block = block->outerBlock;
        if (!block)
            break;
    }
    for (Obj *v = globals.currentFunction->func->args; v; v = v->next) {
        if (v->token->len == len && memcmp(v->token->str, name, (size_t)len) == 0)
            return v;
    }
    return NULL;
}

Obj *findFunction(const char *name, int len) {
    for (Obj *obj = globals.functions; obj; obj = obj->next) {
        if (obj->token->len == len && memcmp(obj->token->str, name, (size_t)len) == 0)
            return obj;
    }
    return NULL;
}

// Look up structure and return it.  If structure didn't found, returns NULL.
static Struct *findStruct(const char *name, int len) {
    for (Node *block = globals.currentBlock; block; block = block->outerBlock) {
        for (Struct *s = block->structs; s; s = s->next) {
            if (s->tagName->len == len &&
                    memcmp(s->tagName->str, name, (size_t)len) == 0)
                return s;
        }
    }
    for (Struct *s = globals.structs; s; s = s->next) {
        if (s->tagName->len == len &&
                memcmp(s->tagName->str, name, (size_t)len) == 0)
            return s;
    }
    return NULL;
}

// Search member in struct.  Returns the member if found, otherwise NULL.
Obj *findStructMember(const Struct *s, const char *name, int len) {
    for (Obj *m = s->members; m; m = m->next) {
        if (m->token->len == len && memcmp(m->token->str, name, (size_t)len) == 0)
            return m;
    }
    return NULL;
}

static TypeInfo *newTypeInfo(TypeKind kind) {
    TypeInfo *t = (TypeInfo *)safeAlloc(sizeof(TypeInfo));
    t->type = kind;
    return t;
}

// Parse base of declared type and return its information.  Return NULL if type
// doesn't appear.
static TypeInfo *parseBaseType(void) {
    Token *type = consumeTypeName();
    if (type)
        return newTypeInfo(type->varType);

    if (consumeCertainTokenType(TokenStruct)) {
        Token *tagName = consumeIdent();
        Struct *s = findStruct(tagName->str, tagName->len);
        TypeInfo *typeInfo = NULL;
        if (!s)
            errorAt(tagName->str, "Unknown struct");

        typeInfo = newTypeInfo(TypeStruct);
        typeInfo->structEntity = s;
        return typeInfo;
    }

    return NULL;
}

static Obj *parseEntireDeclaration(int allowTentativeArray) {
    TypeInfo *baseType = parseBaseType();
    Obj *obj = parseAdvancedTypeDeclaration(baseType, allowTentativeArray);
    return obj;
}

static Obj *parseAdvancedTypeDeclaration(TypeInfo *baseType, int allowTentativeArray) {
    Obj *obj = NULL;
    TypeInfo *placeHolder = NULL;
    Token *ident = NULL;

    obj = (Obj *)safeAlloc(sizeof(Obj));

    while (consumeReserved("*")) {
        TypeInfo *tmp = NULL;
        tmp = newTypeInfo(TypePointer);
        tmp->baseType = baseType;
        baseType = tmp;
    }

    ident = consumeIdent();
    if (ident) {
        obj->token = ident;
        obj->type = baseType;
    } else if (consumeReserved("(")) {
        // TODO: Free inner object
        Obj *inner = NULL;
        placeHolder = newTypeInfo(TypeNone);
        inner = parseAdvancedTypeDeclaration(placeHolder, 0);
        obj->type = inner->type;
        obj->token = inner->token;
        inner->token = NULL;
        expectReserved(")");
    }
    // TODO: Give error "ident expected" here?

    if (matchReserved("[")) {
        TypeInfo *arrayType = NULL;
        TypeInfo **curType = &arrayType;
        int needSize = !allowTentativeArray;
        while (consumeReserved("[")) {
            int arraySize = -1;
            if (needSize) {
                arraySize = expectNumber();
            } else {
                consumeNumber(&arraySize);
            }
            expectReserved("]");

            *curType = newTypeInfo(TypeArray);
            (*curType)->arraySize = arraySize;
            curType = &(*curType)->baseType;
            needSize = 1;
        }
        *curType = newTypeInfo(baseType->type);
        **curType = *baseType;
        baseType = arrayType;
    } else if (matchReserved("(")) {
        TypeInfo *funcType = NULL;

        obj->func = parseFuncArgDeclaration();
        obj->func->retType = baseType;

        funcType = newTypeInfo(TypeFunction);
        funcType->retType = baseType;
        if (obj->func->args)
            funcType->argTypes = obj->func->args->type;
        baseType = funcType;
    }
    if (placeHolder)
        *placeHolder = *baseType;
    else
        obj->type = baseType;
    return obj;
}

static Function *parseFuncArgDeclaration(void) {
    Function *func = (Function *)safeAlloc(sizeof(Function));
    Obj **arg = &func->args;

    expectReserved("(");
    if (consumeReserved(")"))
        return func;

    for (;;) {
        Token *tokenSave = globals.token;
        if (consumeReserved("...")) {
            func->haveVaArgs = 1;
            break;
        }
        if (*arg) {
            if ((*arg)->next)
                (*arg)->type->next = (*arg)->next->type;
            arg = &(*arg)->next;
        }
        (*arg) = parseEntireDeclaration(1);
        if (!(*arg) || (*arg)->type->type == TypeVoid) {
            if (*arg && func->argsCount) {
                errorAt(tokenSave->str,
                        "Function parameter must be only one if \"void\" appears.");
            }
            *arg = NULL;
            break;
        }

        func->argsCount++;

        if (!consumeReserved(","))
            break;
    }

    expectReserved(")");
    return func;
}

// Return size of given type.  If computing failed, exit program.
int sizeOf(const TypeInfo *ti) {
    if (ti->type == TypeInt || ti->type == TypeNumber) {
        return 4;
    } else if (ti->type == TypeChar || ti->type == TypeVoid) {
        return 1;
    } else if (ti->type == TypePointer) {
        return 8;
    } else if (ti->type == TypeArray) {
        return sizeOf(ti->baseType) * ti->arraySize;
    } else if (ti->type == TypeStruct) {
        return ti->structEntity->totalSize;
    }
    errorUnreachable();
}

// Return alignment of given type.  If computing failed, exit program.
// The except is struct.  If it's not able to compute alignment of the struct
// now, returns -1.
static int alignOf(const TypeInfo *ti) {
    if (ti->type == TypeChar || ti->type == TypeVoid) {
        return 1;
    } else if (ti->type == TypeInt || ti->type == TypeNumber) {
        return 4;
    } else if (ti->type == TypePointer) {
        return 8;
    } else if (ti->type == TypeArray) {
        return alignOf(ti->baseType);
    } else if (ti->type == TypeStruct) {
        int align;

        // Alignment of struct with no members is 1.
        if (ti->structEntity->members == NULL)
            return 1;

        align = -1;
        for (Obj *m = ti->structEntity->members; m; m = m->next) {
            int a = alignOf(m->type);
            if (a > align)
                align = a;
        }
        return align;
    }
    errorUnreachable();
}

// Determine the type of expression result for arithmetic operands.
// For the details, see the "6.3.1 Arithmetic operands" chapter (P.68) in this
// C11 draft PDF:
// https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1548.pdf
static TypeInfo *getTypeForArithmeticOperands(TypeInfo *lhs, TypeInfo *rhs) {
    // TODO: Suport missing types: e.g. pointer
    if (lhs->type == rhs->type) {
        return lhs;
    } else if (lhs->type == TypeNumber) {
        return rhs;
    } else if (rhs->type == TypeNumber) {
        return lhs;
    } else if (lhs->type == TypeInt) {
        return lhs;
    } else if (rhs->type == TypeInt) {
        return rhs;
    }
    errorUnreachable();
}

void program(void) {
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

// Parse declarations of global variables/functions/structs and definitions of
// functions.
static Node *decl(void) {
    TypeInfo *baseType = NULL;
    Struct *s = NULL;
    Obj *obj = NULL;
    Token *tokenBaseType = NULL;
    int acceptFuncDefinition = 1;

    // Parse and skip struct declarations
    s = structDeclaration();
    if (s) {
        s->next = globals.structs;
        globals.structs = s;
        return NULL;
    }

    tokenBaseType = globals.token;
    baseType = parseBaseType();
    if (!baseType) {
        errorTypeExpected();
    }
    for (;;) {
        Token *tokenObjHead = globals.token;
        obj = parseAdvancedTypeDeclaration(baseType, 1);
        if (!obj->token) {
            errorAt(tokenObjHead->str, "Missing variable/function name.");
        }
        if (obj->type->type == TypeFunction && matchReserved("{")) {
            // Function definition.
            Obj *funcFound = NULL;
            Node *n = NULL;
            int argOffset = 0;
            int argNum = 0;

            if (!acceptFuncDefinition) {
                errorAt(globals.token->str, "Function definition is not expected here.");
            }

            for (Obj *arg = obj->func->args; arg; arg = arg->next) {
                if (!arg->token)
                    errorAt(tokenObjHead->str, "Argument name is missed.");
                else if (arg->type->type == TypeVoid)
                    errorAt(tokenObjHead->str,
                            "Cannot declare function argument with type \"void\"");
            }

            funcFound = findFunction(obj->token->str, obj->token->len);
            if (funcFound) {
                if (funcFound->func->haveImpl) {
                    errorAt(tokenObjHead->str, "Redefinition of function.");
                }

                // Check types are same with previous declaration.
                if (!checkTypeEqual(funcFound->func->retType, obj->func->retType)) {
                    errorAt(tokenBaseType->str,
                            "Function return type mismatch with previous declaration.");
                } else if (funcFound->func->argsCount != obj->func->argsCount ||
                        funcFound->func->haveVaArgs != obj->func->haveVaArgs) {
                    errorAt(tokenObjHead->str,
                            "Mismatch number of function arguments with previous declaration.");
                } else {
                    Obj *argDecl = funcFound->func->args;
                    Obj *argDef = obj->func->args;
                    for (; argDecl; argDecl = argDecl->next, argDef = argDef->next) {
                        if (!checkTypeEqual(argDecl->type, argDef->type))
                            errorAt(argDef->token->str,
                                    "Argument type mismatch with previous declaration.");
                    }
                }

                // Argument name may be omitted with function declaration.
                // Function implementation must have argument name, so
                // replace it to make sure argument name can be found.
                // TODO: Free funcFound->args
                funcFound->func->args = obj->func->args;
                funcFound->func->haveImpl = 1;
            } else {
                // Register function
                obj->next = globals.functions;
                globals.functions = obj;
            }
            // TODO: Free n->func
            n = newNodeFunction(obj->token);
            n->func = obj;

            // Dive into this function block.
            globals.currentBlock = n;
            globals.currentFunction = obj;

            // Compute argument variables' offset.
            // Note that arguments are all copied onto stack at the head of
            // function.
            argOffset = 0;
            argNum = 0;
            for (Obj *v = obj->func->args; v; v = v->next) {
                ++argNum;
                if (argNum > REG_ARGS_MAX_COUNT) {
                    argOffset -= ONE_WORD_BYTES;
                    v->offset = argOffset;
                } else {
                    argOffset += sizeOf(v->type);
                    v->offset = argOffset;
                    n->localVarSize = argOffset;
                    if (argNum == REG_ARGS_MAX_COUNT) {
                        argOffset = -ONE_WORD_BYTES;
                    }
                }
            }

            // Handle function body
            n->body = stmt();

            // Escape from this function block to the outer one.
            globals.currentBlock = n->outerBlock;

            return n;
        } else if (obj->type->type == TypeFunction) {
            // Function declaration
            Obj *funcFound = NULL;
            funcFound = findFunction(obj->token->str, obj->token->len);
            if (funcFound) {
                // Check types are same with previous declaration.
                if (!checkTypeEqual(funcFound->func->retType, obj->func->retType)) {
                    errorAt(tokenBaseType->str,
                            "Function return type mismatch with previous declaration.");
                } else if (funcFound->func->argsCount != obj->func->argsCount ||
                        funcFound->func->haveVaArgs != obj->func->haveVaArgs) {
                    errorAt(tokenObjHead->str,
                            "Mismatch number of function arguments with previous declaration.");
                } else {
                    Obj *argDecl = funcFound->func->args;
                    Obj *argDef = obj->func->args;
                    for (; argDecl; argDecl = argDecl->next, argDef = argDef->next) {
                        if (!checkTypeEqual(argDecl->type, argDef->type))
                            errorAt(tokenObjHead->str,
                                    "Argument type mismatch with previous declaration.");
                    }
                }
            } else {
                // Register function
                obj->func->haveImpl = 0;
                obj->next = globals.functions;
                globals.functions = obj;
            }
        } else {
            // Global variable declaration
            if (findGlobalVar(obj->token->str, obj->token->len))
                errorAt(obj->token->str, "Redefinition of variable.");
            else if (obj->type->type == TypeVoid)
                errorAt(obj->token->str,
                        "Cannot declare variable with type \"void\"");

            obj->next = globals.vars;
            globals.vars = obj;
        }

        if (!consumeReserved(","))
            break;

        acceptFuncDefinition = 0;
    }
    expectReserved(";");
    return NULL;
}

static Node *stmt(void) {
    Node *varDeclNode = NULL;
    Struct *s = NULL;

    // Parse and skip struct declarations
    s = structDeclaration();
    if (s) {
        s->next = globals.currentBlock->structs;
        globals.currentBlock->structs = s;
        return NULL;
    }

    varDeclNode = varDeclaration();
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
            if (last->next)
                last = last->next;
        }
        n->body = body.next;
        globals.currentBlock = n->outerBlock;  // Escape from this block to the outer one.
        return n;
    } else if (consumeCertainTokenType(TokenBreak)) {
        Node *n = newNode(NodeBreak, &Types.None);
        expectReserved(";");
        return n;
    } else if (consumeCertainTokenType(TokenContinue)) {
        Node *n = newNode(NodeContinue, &Types.None);
        expectReserved(";");
        return n;
    } else if (consumeCertainTokenType(TokenReturn)) {
        Node *n = expr();
        expectReserved(";");
        n = newNodeBinary(NodeReturn, n, NULL, n->type);
        n->token = n->token->prev;
        return n;
    } else if (consumeCertainTokenType(TokenIf)) {
        Node *n = newNode(NodeIf, &Types.None);
        Node elseblock;
        Node *lastElse = &elseblock;
        elseblock.next = NULL;
        n->blockID = globals.blockCount++;

        expectReserved("(");
        n->condition = expr();
        expectReserved(")");
        n->body = stmt();

        while (consumeCertainTokenType(TokenElseif)) {
            Node *e = newNode(NodeElseif, &Types.None);
            expectReserved("(");
            e->condition = expr();
            expectReserved(")");
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
        // Creating new block here is to support variable declarations in
        // for-statement's initializer.
        // In detail, we compile the following code
        //    for (int i = 0; i < 10; ++i) {...}
        // like this:
        //    {
        //        int i = 0;
        //        for (; i < 10; ++i) {...}
        //    }
        Node *block = newNode(NodeBlock, &Types.None);
        Node *n;

        // Dive into this `for` block.
        globals.currentBlock = block;

        n = newNodeFor();
        block->body = n;

        expectReserved("(");
        if (!consumeReserved(";")) {
            Token *tokenSave = globals.token;
            if (parseBaseType()) {
                Node *block = newNode(NodeBlock, &Types.None);

                globals.token = tokenSave;
                block->body = varDeclaration();

                n->initializer = block;
            } else {
                n->initializer = expr();
                expectReserved(";");
            }
        }
        if (!consumeReserved(";")) {
            n->condition = expr();
            expectReserved(";");
        }
        if (!consumeReserved(")")) {
            n->iterator = expr();
            expectReserved(")");
        }
        n->body = stmt();

        // Escape this block.
        globals.currentBlock = block->outerBlock;

        return block;
    } else if (consumeCertainTokenType(TokenWhile)) {
        Node *n = newNodeFor();
        expectReserved("(");
        n->condition = expr();
        expectReserved(")");
        n->body = stmt();
        return n;
    } else if (consumeCertainTokenType(TokenDo)) {
        Node *n = newNodeFor();
        n->kind = NodeDoWhile;
        n->body = stmt();
        if (!consumeCertainTokenType(TokenWhile)) {
            errorAt(globals.token->prev->str, "\"while\" expected.");
        }
        expectReserved("(");
        n->condition = expr();
        expectReserved(")");
        expectReserved(";");
        return n;
    } else {
        Node *n = expr();
        expectReserved(";");
        return n;
    }
}

// Parse struct declaration.  Returns TRUE if there's a struct declaration,
// otherwise FALSE.
static Struct *structDeclaration(void) {
    Token *tokenSave = globals.token;
    Token *tagName = NULL;
    Struct *s = NULL;

    if (!consumeCertainTokenType(TokenStruct)) {
        return NULL;
    }

    tagName = consumeIdent();

    if (!matchReserved("{")) {
        globals.token = tokenSave;
        return NULL;
    }

    s = findStruct(tagName->str, tagName->len);
    if (s)
        errorAt(tagName->str, "Redefinition of struct");

    s = newStruct();
    s->tagName = tagName;

    structBody(s);

    expectReserved(";");
    return s;
}

static void structBody(Struct *s) {
    Obj memberHead;
    Obj *members = &memberHead;
    int structAlign = 1;
    memberHead.next = NULL;
    expectReserved("{");
    for (;;) {
        TypeInfo *baseType = parseBaseType();
        if (!baseType)
            break;

        for (;;) {
            Token *tokenMember = globals.token;
            Obj *member = parseAdvancedTypeDeclaration(baseType, 0);

            if (!member->token) {
                errorAt(tokenMember->str, "Member name required.");
            } else if (member->type->type == TypeStruct &&
                    member->type->structEntity == s) {
                errorAt(tokenMember->str, "Cannot use struct itself for member type.");
            }

            members->next = member;
            members = members->next;

            if (!consumeReserved(","))
                break;
        }
        expectReserved(";");
    }
    expectReserved("}");

    s->members = memberHead.next;

    s->totalSize = 0;
    for (Obj *m = s->members; m; m = m->next) {
        int size = sizeOf(m->type);
        int align, padding;
        if (size < 0)
            errorAt(m->token->str, "Cannot determine size of this.");
        align = alignOf(m->type);
        padding = s->totalSize % align;
        if (padding)
            padding = align - padding;
        m->offset = s->totalSize + padding;
        s->totalSize += padding + size;

        if (align > structAlign)
            structAlign = align;
    }

    // Add padding.
    if (s->totalSize)
        s->totalSize = (((s->totalSize - 1) / structAlign) + 1) * structAlign;
}

// Parse local variable declaration. If there's no variable declaration,
// returns NULL.
static Node *varDeclaration(void) {
    Node *initblock = newNode(NodeBlock, &Types.None);
    Node headNode;
    Node *n = &headNode;
    Obj *lvar = NULL;
    int totalVarSize = 0;
    int currentVarSize = 0;
    TypeInfo *baseType = NULL;
    TypeInfo *varType = NULL;

    headNode.next = NULL;

    baseType = parseBaseType();
    if (!baseType)
        return NULL;

    for (Node *block = globals.currentBlock; block; block = block->outerBlock) {
        totalVarSize += block->localVarSize;
    }


    for (;;) {
        Token *tokenVar = globals.token;
        Node *varNode = NULL;
        Obj *varObj = NULL;
        int varPadding = 0;
        int varAlignment = 0;

        varObj = parseAdvancedTypeDeclaration(baseType, 1);
        if (!varObj->token) {
            globals.token = tokenVar;
            errorIdentExpected();
        }
        varType = varObj->type;

        if (varType->type == TypeVoid) {
            errorAt(varObj->token->str, "Cannot declare variable with type \"void\"");
        }

        lvar = findLVar(varObj->token->str, varObj->token->len);
        if (lvar) {
            errorAt(varObj->token->str, "Redefinition of variable");
        }

        // Variable size is not defined when array with size omitted, so do not
        // compute variable offset here.  It will be computed after parsing
        // initialzier statement.
        varNode = newNodeLVar(-1, varType);

        // Parse initialzier statement
        if (consumeReserved("=")) {
            Token *tokenAssign = globals.token->prev;
            Node *initializer = NULL;

            initializer = varInitializer();

            if (varType->type == TypeArray) {
                if (initializer->kind == NodeExprList) {
                    n->next = newNodeBinary(NodeClearStack, NULL, varNode, &Types.None);
                    n = n->next;
                    n->next = buildArrayInitNodes(varNode, varType, initializer);
                } else if (initializer->kind == NodeLiteralString) {
                    n->next = buildArrayInitNodes(varNode, varType, initializer);
                } else {
                    errorAt(tokenAssign->next->str,
                            "Initializer-list is expected here.");
                }
            } else {
                if (initializer->kind == NodeExprList) {
                    errorAt(tokenAssign->next->str,
                            "Initializer-list is not expected here.");
                }
                n->next = newNodeBinary(NodeAssign, varNode, initializer, varType);
            }
            while (n->next)
                n = n->next;
        } else if (varType->type == TypeArray && varType->arraySize == -1) {
            errorAt(globals.token->str, "Initializer list required.");
        }


        currentVarSize = sizeOf(varType);
        varAlignment = alignOf(varType);
        varPadding = (totalVarSize + currentVarSize) % varAlignment;
        if (varPadding)
            varPadding = varAlignment - varPadding;

        totalVarSize += varPadding + currentVarSize;
        globals.currentBlock->localVarSize += varPadding + currentVarSize;

        lvar = newObj(varObj->token, varType, totalVarSize);
        varNode->offset = totalVarSize;

        // Register variable.
        if (globals.currentBlock->localVars) {
            lvar->next = globals.currentBlock->localVars;
            globals.currentBlock->localVars = lvar;
        } else {
            globals.currentBlock->localVars = lvar;
        }


        if (!consumeReserved(","))
            break;
    }

    expectReserved(";");

    initblock->body = headNode.next;
    return initblock;
}

static Node *buildArrayInitNodes(Node *varNode, TypeInfo *varType, Node *initializer) {
    if (varType->type == TypeArray) {
        int elemCount = 0;
        int arraySize = varType->arraySize;

        if (initializer->kind == NodeExprList) {
            int index = 0;
            Node head = {};
            Node *node = &head;

            for (Node *elem = initializer->body; elem; elem = elem->next)
                elemCount++;

            if (arraySize == -1) {
                arraySize = elemCount;
                varType->arraySize = arraySize;
            } else if (elemCount > arraySize) {
                errorAt(
                        initializer->token->str,
                        "%d items given to array sized %d.",
                        elemCount,
                        arraySize);
            }

            index = 0;
            for (Node *init = initializer->body; init; init = init->next) {
                Node *elem = newNodeBinary(
                        NodeAdd, varNode, newNodeNum(index), varType);
                index++;
                node->next = buildArrayInitNodes(elem, varType->baseType, init);
                while (node->next)
                    node = node->next;
            }
            return head.next;
        } else if (initializer->kind == NodeLiteralString) {
            // TODO: Check type of lhs is char[] or char*?
            LiteralString *string = initializer->token->literalStr;
            Node head = {};
            Node *node = &head;
            int len = 0;
            int elemIdx = 0;

            if (!string)
                errorUnreachable();

            len = strlen(string->string);

            // Checks for array size is done later.
            for (int i = 0; i < len; ++i) {
                Node *shiftptr = NULL;
                Node *elem = NULL;
                char c = string->string[i];

                if (c == '\\') {
                    char cc = 0;
                    if (checkEscapeChar(string->string[++i], &cc))
                        c = cc;
                    else
                        --i;
                }

                shiftptr = newNodeBinary(
                        NodeAdd, varNode, newNodeNum(elemIdx), varType);
                elem = newNodeBinary(
                        NodeDeref, NULL, shiftptr, varType->baseType);
                node->next = newNodeBinary(
                        NodeAssign, elem, newNodeNum((int)c), elem->type);
                node = node->next;
                elemIdx++;
            }

            elemCount = elemIdx + 1;
            if (arraySize == -1) {
                arraySize = elemCount;
                varType->arraySize = arraySize;
            } else if (elemCount > arraySize) {
                errorAt(
                        initializer->token->str,
                        "%d items given to array sized %d.",
                        elemCount,
                        arraySize);
            } else if (elemCount < arraySize) {
                errorAt(initializer->token->str,
                        "Clearing rest items with 0 is not implemented yet.");
            }

            return head.next;
        } else {
            errorAt(initializer->token->str,
                    "Initializer-list is expected for array");
        }
    } else {
        Node *elem = newNodeBinary(NodeDeref, NULL, varNode, varType);
        return newNodeBinary(NodeAssign, elem, initializer, elem->type);
    }
}

static Node *varInitializer(void) {
    Node *initializer = NULL;

    if (consumeReserved("{")) {
        initializer = newNode(NodeExprList, &Types.None);
        if (!consumeReserved("}")) {
            initializer->body = varInitializerList();
            expectReserved("}");
        }
    } else {
        initializer = assign();
    }
    return initializer;
}

static Node *varInitializerList(void) {
    Node head = {};
    Node *elem = &head;

    for (;;) {
        if (consumeReserved("{")) {
            elem->next = newNode(NodeExprList, &Types.None);
            if (!consumeReserved("}")) {
                elem->next->body = varInitializerList();
                expectReserved("}");
            }
        } else {
            elem->next = assign();
        }
        elem = elem->next;

        if (!consumeReserved(",") || matchReserved("}"))
            break;
    }
    return head.next;
}

static Node *expr(void) {
    Node *list = NULL;
    Node *n = assign();
    if (consumeReserved(",")) {
        list = newNode(NodeExprList, &Types.None);
        list->body = n;
        for (;;) {
            n->next = assign();
            n = n->next;
            if (!consumeReserved(","))
                break;
        }
        list->type = n->type;
    } else {
        list = n;
    }
    return list;
}

static Node *assign(void) {
    Node *n = logicalOR();
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

static Node *logicalOR(void) {
    Node *n = logicalAND();
    while (consumeReserved("||")) {
        n = newNodeBinary(NodeLogicalOR, n, logicalAND(), &Types.Number);
        n->blockID = globals.blockCount++;
    }
    return n;
}

static Node *logicalAND(void) {
    Node *n = equality();
    while (consumeReserved("&&")) {
        n = newNodeBinary(NodeLogicalAND, n, equality(), &Types.Number);
        n->blockID = globals.blockCount++;
    }
    return n;
}

static Node *equality(void) {
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

static Node *relational(void) {
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

static Node *add(void) {
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

static Node *mul(void) {
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

static Node *unary(void) {
    Token *tokenOperator = globals.token;
    Node *n = NULL;
    if (consumeReserved("+")) {
        Node *rhs = postfix();
        n = newNodeBinary(NodeAdd, newNodeNum(0), rhs, rhs->type);
    } else if (consumeReserved("-")) {
        Node *rhs = postfix();
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
    } else if (consumeReserved("!")) {
        Node *rhs = unary();
        n = newNodeBinary(NodeNot, NULL, rhs, rhs->type);
    } else if (consumeCertainTokenType(TokenSizeof)) {
        Token *token_save = globals.token;
        TypeInfo *type = NULL;

        // First, do check for "sizeof(type)".
        if (consumeReserved("(")) {
            type = parseBaseType();
            if (type) {
                Obj *obj = parseAdvancedTypeDeclaration(type, 0);
                type = obj->type;
                expectReserved(")");
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
        return postfix();
    }
    n->token = tokenOperator;
    return n;
}

static Node *postfix(void) {
    Token *tokenSave = globals.token;
    Token *ident = consumeIdent();
    Node *n;

    if (ident && matchReserved("(")) {
        FCall *arg;
        Obj *f;

        f = findFunction(ident->str, ident->len);
        if (!f) {
            errorAt(ident->str, "No such function.");
        }

        n = newNodeFCall(f->func->retType);
        arg = funcArgList();

        n->fcall->name = ident->str;
        n->fcall->len = ident->len;
        n->fcall->args = arg->args;
        n->fcall->argsCount = arg->argsCount;
        n->token = ident;

        safeFree(arg);  // Do NOT free its members! They've been used!
    } else {
        globals.token = tokenSave;
        n = primary();
    }

    for (;;) {
        if (consumeReserved("[")) {
            Node *ex = NULL;
            TypeInfo *exprType = n->type;
            for (;;) {
                if (!isWorkLikePointer(exprType))
                    errorAt(globals.token->prev->str, "Array or pointer is needed.");
                ex = expr();
                expectReserved("]");
                n = newNodeBinary(NodeAdd, n, ex, exprType);
                exprType = exprType->baseType;
                if (!consumeReserved("["))
                    break;
            }
            n = newNodeBinary(NodeDeref, NULL, n, exprType);
        } else if (matchReserved("(")) {
            errorAt(globals.token->str,
                    "Calling returned function object is not supported yet.");
        } else if (consumeReserved("++")) {
            n = newNodeBinary(NodePostIncl, n, NULL, n->type);
        } else if (consumeReserved("--")) {
            n = newNodeBinary(NodePostDecl, n, NULL, n->type);
        } else if (consumeReserved(".") || consumeReserved("->")) {
            Token *ident;
            Obj *member;

            if (globals.token->prev->str[0] == '-') {
                if (n->type->type != TypePointer ||
                        n->type->baseType->type != TypeStruct)
                    errorAt(n->token->str, "Pointer for struct required.");
                n = newNodeBinary(NodeDeref, NULL, n, n->type->baseType);
            }

            if (n->type->type != TypeStruct)
                errorAt(n->token->str, "Struct required.");

            ident = expectIdent();
            member = findStructMember(n->type->structEntity, ident->str, ident->len);
            if (!member)
                errorAt(ident->str, "No such struct member.");

            n = newNodeBinary(NodeMemberAccess, n, NULL, member->type);
        } else {
            break;
        }
    }

    return n;
}

static FCall *funcArgList(void) {
    FCall *fcall = (FCall *)safeAlloc(sizeof(FCall));
    Node *arg = NULL;

    expectReserved("(");
    if (!matchReserved(")")) {
        for (;;) {
            arg = assign();
            arg->next = fcall->args;
            fcall->args = arg;
            ++fcall->argsCount;
            if (!consumeReserved(","))
                break;
        }
    }
    expectReserved(")");

    return fcall;
}

static Node *primary(void) {
    Node *n = NULL;
    Token *ident = NULL;
    Token *string = NULL;

    ident = consumeIdent();
    if (ident) {
        Obj *lvar = findLVar(ident->str, ident->len);
        Obj *gvar = NULL;
        if (lvar) {
            n = newNodeLVar(lvar->offset, lvar->type);
        } else if ((gvar = findGlobalVar(ident->str, ident->len)) != NULL) {
            n = newNode(NodeGVar, gvar->type);
        } else {
            errorAt(ident->str, "Undefined variable");
        }
        n->token = ident;
    } else if (consumeReserved("(")) {
        n = expr();
        expectReserved(")");
    } else if ((string = consumeLiteralString())) {
        TypeInfo *type = newTypeInfo(TypeArray);
        type->arraySize = string->literalStr->len;
        type->baseType = &Types.Char;

        n = newNode(NodeLiteralString, type);
    } else {
        n = newNodeNum(expectNumber());
    }

    return n;
}
