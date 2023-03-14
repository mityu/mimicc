#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mimicc.h"

// TODO: Accept using pointer to unknown type in struct member: e.g.:
//   struct A { struct B *obj };

#define abortIfEOF()  do { if (atEOF()) errorUnexpectedEOF(); } while (0)

static int atEOF(void);
static Obj *parseEntireDeclaration(int allowTentativeArray);
static TypeInfo *parseBaseType(ObjAttr *attr);
static Obj *parseAdvancedTypeDeclaration(TypeInfo *baseType, int allowTentativeArray);
static Function *parseFuncArgDeclaration(void);
static int alignOf(const TypeInfo *ti);
static Node *decl(void);
static Node *stmt(void);
static Struct *structDeclaration(const ObjAttr *attr);
static void structBody(Struct *s);
static Enum *enumDeclaration(const ObjAttr *attr);
static void enumBody(Enum * e);
static Node *varDeclaration(void);
static Node *buildVarInitNodes(Node *varNode, TypeInfo *varType, Node *initializer);
static Node *buildArrayInitNodes(Node *varNode, TypeInfo *varType, Node *initializer);
static Node *buildStructInitNodes(Node *varNode, TypeInfo *varType, Node *initializer);
static Node *varInitializer(void);
static Node *varInitializerList(void);
static Node *expr(void);
static Node *assign(void);
static Node *conditional(void);
static Node *logicalOR(void);
static Node *logicalAND(void);
static Node *XORexpr(void);
static Node *ORexpr(void);
static Node *ANDexpr(void);
static Node *equality(void);
static Node *relational(void);
static Node *shift(void);
static Node *add(void);
static Node *mul(void);
static Node *typecast(void);
static Node *unary(void);
static Node *compoundLiteral(void);
static Node *postfix(void);
static FCall *funcArgList(void);
static Node *primary(void);


_Noreturn static void errorIdentExpected(void) {
    errorAt(globals.token, "An identifier is expected");
}

_Noreturn static void errorTypeExpected(void) {
    errorAt(globals.token, "An type is expected");
}

_Noreturn static void errorUnexpectedEOF(void) {
    errorAt(globals.token, "Unexpected EOF");
}

// Check if name of given token matches to pair of {name, len}.  Returns TRUE
// if so, otherwise FALSE.
int matchToken(const Token *token, const char *name, const int len) {
    return token->len == len && memcmp(token->str, name, len) == 0;
}

// Check if current token matches `op` with type TokenReserved, and returns
// TRUE if so.
static int matchReserved(char *op) {
    abortIfEOF();
    if (globals.token->type == TokenReserved &&
            matchToken(globals.token, op, strlen(op))) {
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
    if (matchReserved(op)) {
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
    errorAt(globals.token, "'%s' is expected.", op);
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
    errorAt(globals.token, "Non number appears.");
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
    errorAt(globals.token, "'%s' is expected.", op);
}

static int atEOF(void) {
    return globals.token->type == TokenEOF;
}

static Token *buildTagNameForNamelessObject(int id) {
    static const char prefix[] = "nameless-object-";
    static const int prefix_size = sizeof(prefix);
    Token *tagName = (Token *)safeAlloc(sizeof(Token));
    int suffix_len = 1;

    for (int tmp = id / 10; tmp; tmp /= 10)
        suffix_len++;

    tagName->len = prefix_size + suffix_len - 1;
    tagName->str = (char *)safeAlloc(tagName->len + 1);

    sprintf(tagName->str, "%s%d", prefix, id);

    return tagName;
}

static void enterNewEnv(void) {
    Env *env = (Env *)safeAlloc(sizeof(Env));
    env->outer = globals.currentEnv;
    globals.currentEnv = env;
}

static void exitCurrentEnv(void) {
    if (!globals.currentEnv->outer)
        errorUnreachable();
    globals.currentEnv = globals.currentEnv->outer;
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

static GVar *newGVar(Obj *obj) {
    GVar *gvar = (GVar *)safeAlloc(sizeof(GVar));
    gvar->obj = obj;
    return gvar;
}

static Struct *newStruct(void) {
    Struct *s = (Struct *)safeAlloc(sizeof(Struct));
    s->totalSize = -1;
    return s;
}

static Typedef *newTypedef(Obj *obj) {
    Typedef *def = (Typedef *)safeAlloc(sizeof(Typedef));
    def->name = obj->token;
    def->type = obj->type;
    return def;
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
    n->env = globals.currentEnv;
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

static Node *newNodeLVar(Obj *obj) {
    Node *n = newNode(NodeLVar, obj->type);
    n->obj = obj;
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
    n->obj = newObjFunction(t);
    n->token = t;
    return n;
}

// Find global variable by name. Return LVar* when variable is found. Returns
// NULL when not.
static GVar *findGlobalVar(char *name, int len) {
    for (GVar *v = globals.globalVars; v; v = v->next) {
        if (matchToken(v->obj->token, name, len))
            return v;
    }
    return NULL;
}

// Find local variable in current block by name. Return LVar* when variable
// found. When not, returns NULL.
// Note that this function does NOT search global variables.
static Obj *findLVar(char *name, int len) {
    for (Env *env = globals.currentEnv; env; env = env->outer) {
        for (Obj *v = env->vars; v; v = v->next) {
            if (matchToken(v->token, name, len))
                return v;
        }
    }
    if (globals.currentFunction) {
        for (Obj *v = globals.currentFunction->func->args; v; v = v->next) {
            if (matchToken(v->token, name, len))
                return v;
        }
    }
    return NULL;
}

Obj *findFunction(const char *name, int len) {
    for (Obj *obj = globals.functions; obj; obj = obj->next) {
        if (matchToken(obj->token, name, len))
            return obj;
    }
    return NULL;
}

// Look up structure and return it.  If structure didn't found, returns NULL.
static Struct *findStruct(const char *name, int len) {
    for (Env *env = globals.currentEnv; env; env = env->outer) {
        for (Struct *s = env->structs; s; s = s->next) {
            if (matchToken(s->tagName, name, len))
                return s;
        }
    }
    return NULL;
}

// Search member in struct.  Returns the member if found, otherwise NULL.
Obj *findStructMember(const Struct *s, const char *name, int len) {
    for (Obj *m = s->members; m; m = m->next) {
        if (matchToken(m->token, name, len))
            return m;
    }
    return NULL;
}

static Enum *findEnum(const char *name, int len) {
    for (Env *env = globals.currentEnv; env; env = env->outer) {
        for (Enum *e = env->enums; e; e = e->next) {
            if (matchToken(e->tagName, name, len))
                return e;
        }
    }
    return NULL;
}

static EnumItem *findEnumItem(const char *name, int len) {
    for (Env *env = globals.currentEnv; env; env = env->outer) {
        for (Enum *e = env->enums; e; e = e->next) {
            for (EnumItem *item = e->items; item; item = item->next) {
                if (matchToken(item->token, name, len))
                    return item;
            }
        }
    }
    return NULL;
}

static Typedef *findTypedef(const char *name, int len) {
    for (Env *env = globals.currentEnv; env; env = env->outer) {
        for (Typedef *def = env->typedefs; def; def = def->next) {
            if (matchToken(def->name, name, len))
                return def;
        }
    }
    return NULL;
}

static TypeInfo *newTypeInfo(TypeKind kind) {
    TypeInfo *t = (TypeInfo *)safeAlloc(sizeof(TypeInfo));
    t->type = kind;
    return t;
}

static TypeInfo *cloneTypeInfo(TypeInfo *type) {
    TypeInfo *clone = newTypeInfo(TypeNone);
    *clone = *type;
    return clone;
}

static GVarInit *newGVarInit(GVarInitKind kind, Node *rhs, int size) {
    GVarInit *init = (GVarInit *)safeAlloc(sizeof(GVarInit));
    init->kind = kind;
    init->rhs = rhs;
    init->size = size;
    return init;
}

// Parse base of declared type and return its information.  Return NULL if type
// doesn't appear.
static TypeInfo *parseBaseType(ObjAttr *attr) {
    Token *type = NULL;

    if (attr) {
        Token *tokenSave = globals.token;
        for (;;) {
            if (consumeCertainTokenType(TokenStatic)) {
                attr->isStatic = 1;
            } else if (consumeCertainTokenType(TokenExtern)) {
                attr->isExtern = 1;
            } else if (consumeCertainTokenType(TokenTypedef)) {
                attr->isTypedef = 1;
            } else {
                break;
            }
        }
        if ((attr->isStatic + attr->isExtern + attr->isTypedef) >= 2) {
            errorAt(tokenSave,
                    "Cannot combine \"static\" ,\"extern\" and \"typedef\".");
        }
    }

    type = consumeTypeName();
    if (type) {
        return newTypeInfo(type->varType);
    } else if (matchCertainTokenType(TokenStruct)) {
        Struct *s = structDeclaration(attr);
        TypeInfo *typeInfo = newTypeInfo(TypeStruct);
        typeInfo->structDef = s;
        return typeInfo;
    } else if (matchCertainTokenType(TokenEnum)) {
        Enum *e = enumDeclaration(attr);
        TypeInfo *typeInfo = newTypeInfo(TypeEnum);
        typeInfo->enumDef = e;
        return typeInfo;
    } else {
        Token *tokenSave = globals.token;
        Token *ident = consumeIdent();

        if (ident) {
            Typedef *def = findTypedef(ident->str, ident->len);

            if (def) {
                return def->type;
            } else {
                globals.token = tokenSave;
                return NULL;
            }
        }
    }
    return NULL;
}

static Obj *parseEntireDeclaration(int allowTentativeArray) {
    TypeInfo *baseType = parseBaseType(NULL);
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
        // TODO: Free 'inner' object
        Obj *inner = NULL;
        placeHolder = newTypeInfo(TypeNone);
        inner = parseAdvancedTypeDeclaration(placeHolder, 0);
        // TODO: Check sizeOf(inner->type)
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
                errorAt(tokenSave,
                        "Function parameter must be only one if \"void\" appears.");
            }
            *arg = NULL;
            break;
        }

        // Read array as pointer when it appears on function arguments.
        for (TypeInfo **type = &(*arg)->type; (*type)->type == TypeArray; type = &(*type)->baseType) {
            *type = cloneTypeInfo(*type);
            (*type)->type = TypePointer;
        }

        func->argsCount++;

        if (!consumeReserved(","))
            break;
    }

    expectReserved(")");
    return func;
}

static void registerTypedef(Typedef *def) {
    if (findTypedef(def->name->str, def->name->len)) {
        errorAt(def->name, "Redefinition of typedef name.");
    }
    def->next = globals.currentEnv->typedefs;
    globals.currentEnv->typedefs = def;
}

// Return size of given type.  Return negative value for incomplete types.  If
// computing failed, exit program.
int sizeOf(const TypeInfo *ti) {
    if (ti->type == TypeInt || ti->type == TypeNumber) {
        return 4;
    } else if (ti->type == TypeEnum) {
        if (ti->enumDef->hasImpl)
            return 4;
        else
            return -1;
    } else if (ti->type == TypeChar || ti->type == TypeVoid) {
        return 1;
    } else if (ti->type == TypePointer) {
        return 8;
    } else if (ti->type == TypeArray) {
        if (ti->arraySize < 0)
            return -1;
        return sizeOf(ti->baseType) * ti->arraySize;
    } else if (ti->type == TypeStruct) {
        return ti->structDef->totalSize;
    }
    errorUnreachable();
}

// Return alignment of given type.  If computing failed, exit program.
// The except is struct.  If it's not able to compute alignment of the struct
// now, returns -1.
static int alignOf(const TypeInfo *ti) {
    if (ti->type == TypeChar || ti->type == TypeVoid) {
        return 1;
    } else if (ti->type == TypeInt || ti->type == TypeNumber || ti->type == TypeEnum) {
        return 4;
    } else if (ti->type == TypePointer) {
        return 8;
    } else if (ti->type == TypeArray) {
        return alignOf(ti->baseType);
    } else if (ti->type == TypeStruct) {
        int align;

        // Alignment of struct with no members is 1.
        if (ti->structDef->members == NULL)
            return 1;

        align = -1;
        for (Obj *m = ti->structDef->members; m; m = m->next) {
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

static int isNamelessObject(Obj *obj) {
    return obj->token->len >= 12 &&
        memcmp(obj->token->str, "nameless-object-", 12) == 0;
}

static GVarInit *buildGVarInitSection(TypeInfo *varType, Node *initializer) {
    if (!initializer) {
        return newGVarInit(GVarInitZero, NULL, sizeOf(varType));
    } else if (varType->type == TypeArray &&
            varType->baseType->type == TypeChar &&
            initializer->kind == NodeLiteralString) {
        GVarInit head = {};
        GVarInit *init = &head;
        int strSize = initializer->token->literalStr->len;
        int arraySize = varType->arraySize;

        if (arraySize < 0)
            varType->arraySize = strSize;
        else if (strSize > arraySize)
            errorAt(initializer->token,
                    "%d items given to array sized %d.", strSize, arraySize);

        init->next = newGVarInit(GVarInitString, initializer, strSize);
        init = init->next;

        if (strSize < arraySize)
            init->next = newGVarInit(GVarInitZero, NULL, arraySize - strSize);

        return head.next;
    } else if (varType->type == TypeArray) {
        GVarInit head = {};
        GVarInit *init = &head;
        int initLen = 0;

        if (initializer->kind != NodeInitList)
            errorAt(initializer->token, "Initialzier-list required");

        for (Node *c = initializer->body; c; c = c->next)
            initLen++;

        if (varType->arraySize < 0)
            varType->arraySize = initLen;
        else if (initLen > varType->arraySize)
            errorAt(initializer->token,
                    "%d items given to array sized %d.", initLen, varType->arraySize);

        for (Node *c = initializer->body; c; c = c->next) {
            init->next = buildGVarInitSection(varType->baseType, c);
            while (init->next)
                init = init->next;
        }

        if (initLen < varType->arraySize)
            init->next = newGVarInit(GVarInitZero, NULL,
                    sizeOf(varType->baseType) * (varType->arraySize - initLen));

        return head.next;
    } else if (varType->type == TypeStruct) {
        GVarInit head = {};
        GVarInit *init = &head;
        Obj *member = NULL;
        int initLen = 0, memberCount = 0;
        int prevOffset = 0;

        // Compound-literal is given.  Re-use the initialzier of it.
        if (checkTypeEqual(varType, initializer->type)) {
            Obj *tmpObj = initializer->obj;

            if (tmpObj->isStatic && isNamelessObject(tmpObj)) {
                for (GVar *var = globals.staticVars; var; var = var->next) {
                    if (matchToken(var->obj->token,
                                tmpObj->token->str, tmpObj->token->len))
                        return var->initializer;
                }
            }
        }

        if (initializer->kind != NodeInitList)
            errorAt(initializer->token, "Initialzier-list required");

        for (Node *c = initializer->body; c; c = c->next)
            initLen++;

        for (member = varType->structDef->members; member; member = member->next)
            memberCount++;

        if (initLen > memberCount)
            errorAt(initializer->token, "Too much items are given.");

        member = varType->structDef->members;
        for (Node *c = initializer->body; c; c = c->next) {
            int offset = member->offset;
            int padding = offset - prevOffset;

            padding = offset - prevOffset;
            if (padding) {
                init->next = newGVarInit(GVarInitZero, NULL, padding);
                init = init->next;
            }

            init->next = buildGVarInitSection(member->type, c);
            while (init->next)
                init = init->next;

            prevOffset = offset + sizeOf(member->type);
            member = member->next;
        }

        if (varType->structDef->totalSize - prevOffset)
            init->next = newGVarInit(
                    GVarInitZero, NULL, varType->structDef->totalSize - prevOffset);

        return head.next;
    } else if (varType->type == TypePointer) {
        int size = sizeOf(varType);

        if (!checkAssignable(varType, initializer->type)) {
            errorAt(initializer->token, "Different type.");
        }

        for (int keepGoing = 1; keepGoing;) {
            switch (initializer->kind) {
            case NodeTypeCast:  // fallthrough.
            case NodeAddress:
                initializer = initializer->rhs;
                break;
            default:
                keepGoing = 0;
                break;
            }
        }

        if (initializer->type->type == TypeArray) {
            if (initializer->kind == NodeGVar ||
                    initializer->kind == NodeLVar ||
                    initializer->kind == NodeLiteralString ||
                    initializer->kind == NodeNum) {
                return newGVarInit(GVarInitPointer, initializer, size);
            }
        } else {
            if (initializer->kind == NodeGVar || initializer->kind == NodeNum ||
                    (initializer->kind == NodeLVar && initializer->obj->isStatic)) {
                return newGVarInit(GVarInitPointer, initializer, size);
            }
        }
        errorAt(initializer->token, "Constant value required.");
    } else {
        int size = sizeOf(varType);
        int modBySize = 0;

        // TODO: This is workaround.
        for (int keepGoing = 1; keepGoing;) {
            switch (initializer->kind) {
            case NodeSub:
                if (initializer->lhs->kind == NodeNum &&
                        initializer->rhs->kind == NodeNum) {
                    initializer =
                        newNodeNum(initializer->lhs->val - initializer->rhs->val);
                } else {
                    keepGoing = 0;
                }
                break;
            default:
                keepGoing = 0;
                break;
            }
        }

        modBySize = initializer->val;
        if (size < 4)
            modBySize &= ((1 << size * 8) - 1);

        if (initializer->kind != NodeNum) {
            errorAt(initializer->token, "Constant value required.");
        } else if (modBySize != initializer->val) {
            errorAt(initializer->token,
                    "Value overflows: value will be %d.", modBySize);
        }
        return newGVarInit(GVarInitNum, initializer, size);
    }
}

void program(void) {
    Node body = {};
    Node *last = &body;

    if (!consumeCertainTokenType(TokenSOF))
        errorUnreachable();

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
    ObjAttr attr = {};
    Token *tokenBaseType = NULL;
    int acceptFuncDefinition = 1;

    tokenBaseType = globals.token;
    baseType = parseBaseType(&attr);
    if (!baseType) {
        errorTypeExpected();
    } else if (baseType->type == TypeStruct || baseType->type == TypeEnum) {
        Token *last = globals.token->prev;

        if (last->len == 1 && last->str[0] == '}' && consumeReserved(";"))
            return NULL;  // Just declaring a struct or an enum.
    }

    for (;;) {
        Token *tokenObjHead = globals.token;

        obj = parseAdvancedTypeDeclaration(baseType, 1);
        obj->isStatic = attr.isStatic;
        obj->isExtern = attr.isExtern;

        if (!obj->token) {
            errorAt(tokenObjHead, "Missing variable/function name.");
        }

        if (attr.isTypedef) {
            expectReserved(";");
            registerTypedef(newTypedef(obj));
            return NULL;
        }

        if (obj->type->type == TypeFunction && matchReserved("{")) {
            // Function definition.
            Obj *funcFound = NULL;
            Node *n = NULL;
            int argOffset = 0;
            int argNum = 0;

            if (!acceptFuncDefinition) {
                errorAt(globals.token, "Function definition is not expected here.");
            }

            for (Obj *arg = obj->func->args; arg; arg = arg->next) {
                if (!arg->token)
                    errorAt(tokenObjHead, "Argument name is missed.");
                else if (arg->type->type == TypeVoid)
                    errorAt(tokenObjHead,
                            "Cannot declare function argument with type \"void\"");
            }

            funcFound = findFunction(obj->token->str, obj->token->len);
            if (funcFound) {
                if (funcFound->func->haveImpl) {
                    errorAt(tokenObjHead, "Redefinition of function.");
                }

                // Check types are same with previous declaration.
                if (!checkTypeEqual(funcFound->func->retType, obj->func->retType)) {
                    errorAt(tokenBaseType,
                            "Function return type mismatch with previous declaration.");
                } else if (funcFound->func->argsCount != obj->func->argsCount ||
                        funcFound->func->haveVaArgs != obj->func->haveVaArgs) {
                    errorAt(tokenObjHead,
                            "Mismatch number of function arguments with previous declaration.");
                } else {
                    Obj *argDecl = funcFound->func->args;
                    Obj *argDef = obj->func->args;
                    for (; argDecl; argDecl = argDecl->next, argDef = argDef->next) {
                        if (!checkTypeEqual(argDecl->type, argDef->type))
                            errorAt(argDef->token,
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
            enterNewEnv();
            n = newNodeFunction(obj->token);
            n->obj = obj;

            globals.currentFunction = obj;

            // Compute argument variables' offset.
            // Note that arguments are all copied onto stack at the head of
            // function.
            if (obj->func->haveVaArgs) {
                for (Obj *v = obj->func->args; v; v = v->next) {
                    ++argNum;
                    if (argNum > REG_ARGS_MAX_COUNT) {
                        v->offset = -ONE_WORD_BYTES * (argNum - REG_ARGS_MAX_COUNT + 1);
                    } else {
                        v->offset =
                            (REG_ARGS_MAX_COUNT - argNum + 1) * ONE_WORD_BYTES;
                    }
                }
                n->env->varSize = REG_ARGS_MAX_COUNT * ONE_WORD_BYTES;
            } else {
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
                        n->env->varSize = argOffset;
                        if (argNum == REG_ARGS_MAX_COUNT) {
                            argOffset = -ONE_WORD_BYTES;
                        }
                    }
                }
            }
            globals.currentFunction->func->capStackSize = n->env->varSize;

            // Handle function body
            n->body = stmt();

            exitCurrentEnv();

            return n;
        } else if (obj->type->type == TypeFunction) {
            // Function declaration
            Obj *funcFound = NULL;
            funcFound = findFunction(obj->token->str, obj->token->len);
            if (funcFound) {
                // Check types are same with previous declaration.
                if (!checkTypeEqual(funcFound->func->retType, obj->func->retType)) {
                    errorAt(tokenBaseType,
                            "Function return type mismatch with previous declaration.");
                } else if (funcFound->func->argsCount != obj->func->argsCount ||
                        funcFound->func->haveVaArgs != obj->func->haveVaArgs) {
                    errorAt(tokenObjHead,
                            "Mismatch number of function arguments with previous declaration.");
                } else {
                    Obj *argDecl = funcFound->func->args;
                    Obj *argDef = obj->func->args;
                    for (; argDecl; argDecl = argDecl->next, argDef = argDef->next) {
                        if (!checkTypeEqual(argDecl->type, argDef->type))
                            errorAt(tokenObjHead,
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
            GVar *gvar = NULL;
            GVar *existingVar = NULL;
            existingVar = findGlobalVar(obj->token->str, obj->token->len);
            if (existingVar && !existingVar->obj->isExtern)
                errorAt(obj->token, "Redefinition of variable.");
            else if (obj->type->type == TypeVoid)
                errorAt(obj->token,
                        "Cannot declare variable with type \"void\"");

            if (existingVar) {
                gvar = existingVar;
                if (!attr.isExtern)
                    gvar->obj->isExtern = 0;
            } else {
                gvar = newGVar(obj);
            }


            if (consumeReserved("=")) {
                Node *init = NULL;

                if (attr.isExtern)
                    errorAt(globals.token->prev,
                            "Extern variable cannot have initializers");

                init = varInitializer();
                gvar->initializer = buildGVarInitSection(obj->type, init);
            } else {
                gvar->initializer = buildGVarInitSection(obj->type, NULL);
            }

            if (sizeOf(obj->type) < 0) {
                errorAt(obj->token, "Variable has incomplete type.");
            }

            if (!existingVar) {
                gvar->next = globals.globalVars;
                globals.globalVars = gvar;
            }
        }

        if (!consumeReserved(","))
            break;

        acceptFuncDefinition = 0;
    }
    expectReserved(";");
    return NULL;
}

static Node *stmt(void) {
    static Node *switchNode = NULL;
    Node *varDeclNode = NULL;

    varDeclNode = varDeclaration();
    if (varDeclNode)
        return varDeclNode;

    if (consumeReserved("{")) {
        Node *n;
        Node body = {};
        Node *last = &body;

        enterNewEnv();
        n = newNode(NodeBlock, &Types.None);
        while (!consumeReserved("}")) {
            last->next = stmt();
            if (last->next)
                last = last->next;
        }
        n->body = body.next;
        exitCurrentEnv();
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
        Node *n = NULL;
        Node *expression = NULL;
        Token *tokenReturn = globals.token->prev;
        if (!consumeReserved(";")) {
            expression = expr();
            expectReserved(";");
        }
        n = newNodeBinary(NodeReturn, expression, NULL, &Types.Void);
        n->token = tokenReturn;
        if (expression)
            n->type = expression->type;
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
    } else if (consumeCertainTokenType(TokenSwitch)) {
        Node *switchNodeSave = switchNode;
        Node *n = newNode(NodeSwitch, &Types.None);

        switchNode = n;
        n->blockID = globals.blockCount++;
        expectReserved("(");
        n->condition = expr();
        expectReserved(")");
        n->body = stmt();
        switchNode = switchNodeSave;
        return n;
    } else if (consumeCertainTokenType(TokenCase)) {
        Node *n;
        SwitchCase *thisCase;

        if (!switchNode)
            errorAt(globals.token->prev,
                    "\"case\" label not within a switch statement");

        n = newNode(NodeSwitchCase, &Types.None);
        n->condition = expr();
        n->blockID = switchNode->blockID;
        expectReserved(":");

        if (n->condition->kind != NodeNum)
            errorAt(n->token, "Constant expression required.");

        thisCase = (SwitchCase*)safeAlloc(sizeof(SwitchCase));
        thisCase->node = n;

        if (switchNode->cases) {
            SwitchCase *cases = switchNode->cases;
            while (cases->next)
                cases = cases->next;
            cases->next = thisCase;
        } else {
            switchNode->cases = thisCase;
        }

        return n;
    } else if (consumeCertainTokenType(TokenDefault)) {
        Node *n;
        SwitchCase *thisCase;

        expectReserved(":");
        if (!switchNode)
            errorAt(globals.token->prev,
                    "\"default\" label not within a switch statement");

        n = newNode(NodeSwitchCase, &Types.None);
        n->blockID = switchNode->blockID;

        thisCase = (SwitchCase*)safeAlloc(sizeof(SwitchCase));
        thisCase->node = n;

        if (switchNode->cases) {
            SwitchCase *cases = switchNode->cases;
            while (cases->next)
                cases = cases->next;
            cases->next = thisCase;
        } else {
            switchNode->cases = thisCase;
        }

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
        Node *block;
        Node *n;

        enterNewEnv();
        block = newNode(NodeBlock, &Types.None);

        n = newNodeFor();
        block->body = n;

        expectReserved("(");
        if (!consumeReserved(";")) {
            Token *tokenSave = globals.token;
            if (parseBaseType(NULL)) {
                Node *blockInit = newNode(NodeBlock, &Types.None);

                globals.token = tokenSave;
                blockInit->body = varDeclaration();

                n->initializer = blockInit;
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

        exitCurrentEnv();

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
            errorAt(globals.token->prev, "\"while\" expected.");
        }
        expectReserved("(");
        n->condition = expr();
        expectReserved(")");
        expectReserved(";");
        return n;
    } else if (consumeReserved(";")) {
        // Empty statement
        return NULL;
    } else {
        Node *n = expr();
        expectReserved(";");
        return n;
    }
}

// Parse struct declaration.  Returns struct if there's a struct declaration,
// otherwise NULL.
static Struct *structDeclaration(const ObjAttr *attr) {
    // TODO: Prohibit declaring new enum at function parameter.
    Token *tokenStruct = globals.token;
    Token *tagName = NULL;
    int allowUndefinedStruct = attr && attr->isTypedef;

    if (!consumeCertainTokenType(TokenStruct)) {
        return NULL;
    }

    tagName = consumeIdent();

    if (matchReserved("{")) {
        Struct *s = NULL;
        int registerStruct = 0;
        if (tagName) {
            s = findStruct(tagName->str, tagName->len);
            if (s && s->hasImpl) {
                errorAt(tokenStruct, "Redefinition of struct.");
            }
        } else {
            tagName = buildTagNameForNamelessObject(globals.namelessStructCount++);
        }
        if (!s) {
            s = newStruct();
            registerStruct = 1;
        }
        s->tagName = tagName;
        s->hasImpl = 1;
        structBody(s);
        if (registerStruct) {
            s->next = globals.currentEnv->structs;
            globals.currentEnv->structs = s;
        }

        return s;
    } else if (tagName) {
        Struct *s = findStruct(tagName->str, tagName->len);
        if (allowUndefinedStruct) {
            if (!s) {
                s = newStruct();
                s->tagName = tagName;
                s->hasImpl = 0;
                s->next = globals.currentEnv->structs;
                globals.currentEnv->structs = s;
            }
        } else if (!(s && s->hasImpl)) {
            errorAt(tokenStruct, "Undefiend struct.");
        }
        return s;
    } else {
        errorAt(globals.token, "Missing struct tag name.");
    }
}

static void structBody(Struct *s) {
    Obj memberHead;
    Obj *members = &memberHead;
    int structAlign = 1;
    memberHead.next = NULL;
    expectReserved("{");
    for (;;) {
        TypeInfo *baseType = parseBaseType(NULL);
        if (!baseType)
            break;

        for (;;) {
            Token *tokenMember = globals.token;
            Obj *member = parseAdvancedTypeDeclaration(baseType, 0);

            if (!member->token) {
                errorAt(tokenMember, "Member name required.");
            } else if (member->type->type == TypeStruct &&
                    member->type->structDef == s) {
                errorAt(tokenMember, "Cannot use struct itself for member type.");
            }

            for (Obj *m = memberHead.next; m; m = m->next) {
                if (matchToken(m->token, member->token->str, member->token->len))
                    errorAt(member->token, "Duplicate member name.");
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
            errorAt(m->token, "Cannot determine size of this.");
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

// Parse enum declaration if found and returns parse result.  If no enum found,
// returns NULL.
static Enum *enumDeclaration(const ObjAttr *attr) {
    Token *tokenEnum = globals.token;
    Token *tagName = NULL;
    int allowUndefinedEnum = attr && attr->isTypedef;

    if (!consumeCertainTokenType(TokenEnum))
        return NULL;

    tagName = consumeIdent();

    // TODO: Prohibit declaring new enum at function parameter.
    if (matchReserved("{")) {
        Enum *e = NULL;
        int registerEnum = 0;
        if (tagName) {
            e = findEnum(tagName->str, tagName->len);
            if (e && e->hasImpl)
                errorAt(tokenEnum, "Redefinition of enum.");
        } else {
            tagName = buildTagNameForNamelessObject(globals.namelessEnumCount++);
        }
        if (!e) {
            e = (Enum *)safeAlloc(sizeof(Enum));
            registerEnum = 1;
        }
        e->tagName = tagName;
        e->hasImpl = 1;
        enumBody(e);
        if (registerEnum) {
            e->next = globals.currentEnv->enums;
            globals.currentEnv->enums = e;
        }
        return e;
    } else if (tagName) {
        Enum *e = findEnum(tagName->str, tagName->len);
        if (allowUndefinedEnum) {
            if (!e) {
                e = (Enum *)safeAlloc(sizeof(Enum));
                e->tagName = tagName;
                e->hasImpl = 0;
                e->next = globals.currentEnv->enums;
                globals.currentEnv->enums = e;
            }
        } else if (!(e && e->hasImpl)) {
            errorAt(tokenEnum, "Undefined enum.");
        }
        return e;
    } else {
        errorAt(globals.token, "Missing enum tag name.");
    }
}

static void enumBody(Enum *e) {
    EnumItem head = {};
    EnumItem *item = &head;
    int value = 0;

    expectReserved("{");
    for (;;) {
        Token *itemToken = consumeIdent();
        EnumItem *previous = NULL;
        if (!itemToken)
            break;

        previous = findEnumItem(itemToken->str, itemToken->len);
        if (previous)
            errorAt(itemToken, "Duplicate enum item.");

        for (EnumItem *p = head.next; p; p = p->next)
            if (matchToken(p->token, itemToken->str, itemToken->len))
                errorAt(itemToken, "Duplicate enum item.");

        item->next = (EnumItem *)safeAlloc(sizeof(EnumItem));
        item = item->next;
        item->token = itemToken;
        item->value = value++;

        if (!consumeReserved(","))
            break;
    }
    expectReserved("}");
    e->items = head.next;
}

// Parse local variable declaration. If there's no variable declaration,
// returns NULL.
static Node *varDeclaration(void) {
    Node *initNode = NULL;
    Node headNode = {};
    Node *n = &headNode;
    ObjAttr attr = {};
    int totalVarSize = 0;
    int currentVarSize = 0;
    TypeInfo *baseType = NULL;
    TypeInfo *varType = NULL;

    baseType = parseBaseType(&attr);
    if (!baseType) {
        return NULL;
    } else if (baseType->type == TypeStruct || baseType->type == TypeEnum) {
        Token *last = globals.token->prev;

        // Do not consume ";" here; it's handled by stmt().
        if (last->str[0] == '}' && last->len == 1 && matchReserved(";"))
            return NULL;  // Just declaring a struct or an enum.
    }

    for (Env *env = globals.currentEnv; env; env = env->outer) {
        totalVarSize += env->varSize;
    }


    for (;;) {
        Token *tokenVar = globals.token;
        Node *varNode = NULL;
        Obj *varObj = NULL;
        Obj *existingVar = NULL;
        GVar *gvarObj = NULL;
        int varPadding = 0;
        int varAlignment = 0;

        varObj = parseAdvancedTypeDeclaration(baseType, 1);
        if (!varObj->token) {
            globals.token = tokenVar;
            errorIdentExpected();
        }
        if (attr.isTypedef) {
            expectReserved(";");
            registerTypedef(newTypedef(varObj));
            return newNode(NodeNop, &Types.None);
        }

        varObj->offset = -1;
        varObj->isStatic = attr.isStatic;
        varObj->isExtern = attr.isExtern;
        varType = varObj->type;

        if (varType->type == TypeVoid) {
            errorAt(varObj->token, "Cannot declare variable with type \"void\"");
        }

        existingVar = findLVar(varObj->token->str, varObj->token->len);
        if (existingVar) {
            errorAt(varObj->token, "Redefinition of variable");
        }

        // Variable size is not defined when array with size omitted, so do not
        // compute variable offset here.  It will be computed after parsing
        // initializer statement.
        varNode = newNodeLVar(varObj);
        varNode->obj = varObj;
        if (varObj->isStatic) {
            gvarObj = newGVar(varObj);
            varObj->staticVarID = globals.staticVarCount++;
        }

        // Parse initializer statement
        if (consumeReserved("=")) {
            Node *initializer = NULL;

            if (attr.isExtern)
                errorAt(globals.token->prev,
                        "Extern variable cannot have initializers");

            initializer = varInitializer();

            if (varObj->isStatic) {
                gvarObj->initializer = buildGVarInitSection(varType, initializer);
            } else {
                n->next = newNodeBinary(NodeClearStack, NULL, varNode, &Types.None);
                n = n->next;
                n->next = buildVarInitNodes(varNode, varType, initializer);

                while (n->next)
                    n = n->next;
            }
        } else if (varType->type == TypeArray && varType->arraySize == -1) {
            errorAt(globals.token, "Initializer list required.");
        }

        if (sizeOf(varType) < 0) {
            errorAt(varObj->token, "Variable has incomplete type.");
        }


        if (varObj->isStatic) {
            // Register static variable.
            gvarObj->next = globals.staticVars;
            globals.staticVars = gvarObj;

            // If no initializer is given, initializer section is not builded
            // yet, so create it here.
            if (!gvarObj->initializer)
                gvarObj->initializer = buildGVarInitSection(varType, NULL);
        } else if (varObj->isExtern) {
            // Do nothing.
        } else {
            currentVarSize = sizeOf(varType);
            varAlignment = alignOf(varType);
            varPadding = (totalVarSize + currentVarSize) % varAlignment;
            if (varPadding)
                varPadding = varAlignment - varPadding;

            totalVarSize += varPadding + currentVarSize;
            globals.currentEnv->varSize += varPadding + currentVarSize;

            varObj->offset = totalVarSize;
        }

        // Register variable.
        varObj->next = globals.currentEnv->vars;
        globals.currentEnv->vars = varObj;


        if (!consumeReserved(","))
            break;
    }

    expectReserved(";");

    if (globals.currentFunction->func->capStackSize < totalVarSize)
        globals.currentFunction->func->capStackSize = totalVarSize;

    initNode = newNode(NodeBlock, &Types.None);
    initNode->body = headNode.next;
    return initNode;
}

static Node *buildVarInitNodes(Node *varNode, TypeInfo *varType, Node *initializer) {
    Node head = {};
    Node *n = &head;
    if (varType->type == TypeArray) {
        if (initializer->kind == NodeInitList || initializer->kind == NodeLiteralString) {
            n->next = buildArrayInitNodes(varNode, varType, initializer);
        } else {
            errorAt(initializer->token,
                    "Initializer-list is expected here.");
        }
    } else if (varType->type == TypeStruct) {
        if (initializer->type->type == TypeStruct)
            n->next = newNodeBinary(NodeAssignStruct, varNode, initializer, varType);
        else
            n->next = buildStructInitNodes(varNode, varType, initializer);
    } else {
        if (initializer->kind == NodeInitList) {
            errorAt(initializer->token,
                    "Initializer-list is not expected here.");
        }
        n->next = newNodeBinary(NodeAssign, varNode, initializer, varType);
    }
    return head.next;
}

static Node *buildArrayInitNodes(Node *varNode, TypeInfo *varType, Node *initializer) {
    if (varType->type == TypeArray) {
        int elemCount = 0;
        int arraySize = varType->arraySize;

        if (initializer->kind == NodeInitList) {
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
                        initializer->token,
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
                        initializer->token,
                        "%d items given to array sized %d.",
                        elemCount,
                        arraySize);
            }

            return head.next;
        } else {
            errorAt(initializer->token,
                    "Initializer-list is expected for array");
        }
    } else {
        Node *elem = newNodeBinary(NodeDeref, NULL, varNode, varType);
        return buildVarInitNodes(elem, elem->type, initializer);
    }
}

static Node *buildStructInitNodes(Node *varNode, TypeInfo *varType, Node *initializer) {
    Node head = {};
    Node *node = &head;
    Node *init = NULL;
    Obj *member = NULL;
    int initLen = 0;
    int memberCount = 0;

    if (varType->type != TypeStruct)
        errorUnreachable();
    else if (initializer->kind != NodeInitList)
        errorAt(initializer->token, "Initializer-list is expected here.");

    for (Node *c = initializer->body; c; c = c->next)
        initLen++;
    for (member = varNode->type->structDef->members; member; member = member->next)
        memberCount++;

    if (initLen > memberCount)
        errorAt(initializer->token, "Too much items are given.");

    member = varNode->type->structDef->members;
    init = initializer->body;
    for (;;) {
        Node *memberNode = NULL;

        if (!(init && member))
            break;

        memberNode = newNodeBinary(NodeMemberAccess, varNode, NULL, member->type);
        memberNode->token = member->token;
        node->next = buildVarInitNodes(memberNode, memberNode->type, init);

        init = init->next;
        member = member->next;
        while (node->next)
            node = node->next;
    }

    return head.next;
}

static Node *varInitializer(void) {
    Node *initializer = NULL;

    if (consumeReserved("{")) {
        initializer = newNode(NodeInitList, &Types.None);
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
            elem->next = newNode(NodeInitList, &Types.None);
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
    Node *n = conditional();
    Token *t = globals.token;
    if (consumeReserved("=")) {
        if (n->type->type == TypeStruct) {
            n = newNodeBinary(NodeAssignStruct, n, assign(), n->type);
        } else {
            n = newNodeBinary(NodeAssign, n, assign(), n->type);
        }
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
    } else if (consumeReserved("*=")) {
        Node *lhs = n;
        Node *rhs = assign();
        TypeInfo *type = getTypeForArithmeticOperands(lhs->type, rhs->type);
        n = newNodeBinary(NodeMul, lhs, rhs, type);
        n->token = t;
        n = newNodeBinary(NodeAssign, lhs, n, lhs->type);
        n->token = t;
    } else if (consumeReserved("/=")) {
        Node *lhs = n;
        Node *rhs = assign();
        TypeInfo *type = getTypeForArithmeticOperands(lhs->type, rhs->type);
        n = newNodeBinary(NodeDiv, lhs, rhs, type);
        n->token = t;
        n = newNodeBinary(NodeAssign, lhs, n, lhs->type);
        n->token = t;
    } else if (consumeReserved("&=")) {
        Node *lhs = n;
        Node *rhs = assign();
        // TODO: Set proper type.
        n = newNodeBinary(NodeBitwiseAND, lhs, rhs, &Types.Number);
        n->token = t;
        n = newNodeBinary(NodeAssign, lhs, n, lhs->type);
        n->token = t;
    } else if (consumeReserved("|=")) {
        Node *lhs = n;
        Node *rhs = assign();
        // TODO: Set proper type.
        n = newNodeBinary(NodeBitwiseOR, lhs, rhs, &Types.Number);
        n->token = t;
        n = newNodeBinary(NodeAssign, lhs, n, lhs->type);
        n->token = t;
    } else if (consumeReserved("^=")) {
        Node *lhs = n;
        Node *rhs = assign();
        // TODO: Set proper type.
        n = newNodeBinary(NodeBitwiseXOR, lhs, rhs, &Types.Number);
        n->token = t;
        n = newNodeBinary(NodeAssign, lhs, n, lhs->type);
        n->token = t;
    } else if (consumeReserved("<<=")) {
        Node *lhs = n;
        Node *rhs = assign();
        // TODO: Set proper type.
        n = newNodeBinary(NodeArithShiftL, lhs, rhs, &Types.Number);
        n->token = t;
        n = newNodeBinary(NodeAssign, lhs, n, lhs->type);
        n->token = t;
    } else if (consumeReserved(">>=")) {
        Node *lhs = n;
        Node *rhs = assign();
        // TODO: Set proper type.
        n = newNodeBinary(NodeArithShiftR, lhs, rhs, &Types.Number);
        n->token = t;
        n = newNodeBinary(NodeAssign, lhs, n, lhs->type);
        n->token = t;
    }
    return n;
}

static Node *conditional(void) {
    static int condOpCount = 0;
    Node *n = logicalOR();

    if (consumeReserved("?")) {
        Node *cond = n;
        Node *lhs = NULL;
        Node *rhs = NULL;

        lhs = expr();
        expectReserved(":");
        rhs = conditional();

        n = newNode(NodeConditional, lhs->type);
        n->condition = cond;
        n->lhs = lhs;
        n->rhs = rhs;
        n->blockID = condOpCount++;
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
    Node *n = XORexpr();
    while (consumeReserved("&&")) {
        n = newNodeBinary(NodeLogicalAND, n, XORexpr(), &Types.Number);
        n->blockID = globals.blockCount++;
    }
    return n;
}

static Node *XORexpr(void) {
    Node *n = ORexpr();
    while (consumeReserved("^")) {
        // TODO: Set proper type.
        n = newNodeBinary(NodeBitwiseXOR, n, ORexpr(), &Types.Number);
    }
    return n;
}

static Node *ORexpr(void) {
    Node *n = ANDexpr();
    while (consumeReserved("|")) {
        // TODO: Set proper type.
        n = newNodeBinary(NodeBitwiseOR, n, ANDexpr(), &Types.Number);
    }
    return n;
}

static Node *ANDexpr(void) {
    Node *n = equality();
    while (consumeReserved("&")) {
        // TODO: Set proper type.
        n = newNodeBinary(NodeBitwiseAND, n, equality(), &Types.Number);
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
    Node *n = shift();
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

static Node *shift(void) {
    Node *n = add();
    for (;;) {
        if (consumeReserved("<<")) {
            // TODO: Set proper type.
            n = newNodeBinary(NodeArithShiftL, n, add(), &Types.Number);
        } else if (consumeReserved(">>")) {
            // TODO: Set proper type.
            n = newNodeBinary(NodeArithShiftR, n, add(), &Types.Number);
        } else {
            break;
        }
    }
    return n;
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
    Node *n = typecast();
    for (;;) {
        Token *t = globals.token;
        if (consumeReserved("*")) {
            Node *rhs = typecast();
            TypeInfo *type = getTypeForArithmeticOperands(n->type, rhs->type);
            n = newNodeBinary(NodeMul, n, rhs, type);
        } else if (consumeReserved("/")) {
            Node *rhs = typecast();
            TypeInfo *type = getTypeForArithmeticOperands(n->type, rhs->type);
            n = newNodeBinary(NodeDiv, n, rhs, type);
        } else if (consumeReserved("%")) {
            Node *rhs = typecast();
            TypeInfo *type = getTypeForArithmeticOperands(n->type, rhs->type);
            n = newNodeBinary(NodeDivRem, n, rhs, type);
        } else {
            return n;
        }
        n->token = t;
    }
}

static Node *typecast(void) {
    Token *tokenSave = globals.token;
    if (consumeReserved("(")) {
        TypeInfo *type = NULL;
        Obj *obj = NULL;
        Node *n = NULL;

        type = parseBaseType(NULL);
        if (!type) {
            globals.token = tokenSave;
            return unary();
        }

        obj = parseAdvancedTypeDeclaration(type, 0);

        if (obj->token)
            errorAt(obj->token, "Unexpected identifier.");

        expectReserved(")");

        if (matchReserved("{")) {  // Compound literal.  Not a type cast.
            globals.token = tokenSave;
            return postfix();
        }


        if (sizeOf(obj->type) < 0) {
            errorAt(tokenSave->next, "Incomplete type.");
        }

        n = newNodeBinary(NodeTypeCast, NULL, typecast(), obj->type);
        n->token = tokenSave;
        return n;
    }
    return unary();
}

static Node *unary(void) {
    Token *tokenOperator = globals.token;
    Node *n = NULL;
    if (consumeReserved("+")) {
        Node *rhs = typecast();
        n = newNodeBinary(NodeAdd, newNodeNum(0), rhs, rhs->type);
    } else if (consumeReserved("-")) {
        Node *rhs = typecast();
        n = newNodeBinary(NodeSub, newNodeNum(0), rhs, rhs->type);
    } else if (consumeReserved("&")) {
        Node *rhs = typecast();
        TypeInfo *type = newTypeInfo(TypePointer);
        type->baseType = rhs->type;
        n = newNodeBinary(NodeAddress, NULL, rhs, type);
    } else if (consumeReserved("*")) {
        Node *rhs = typecast();
        TypeKind typeKind = rhs->type->type;
        if (!(rhs->type && (typeKind == TypePointer || typeKind == TypeArray))) {
            errorAt(tokenOperator,
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
        Node *rhs = typecast();
        n = newNodeBinary(NodeNot, NULL, rhs, rhs->type);
    } else if (consumeCertainTokenType(TokenSizeof)) {
        Token *tokenSave = globals.token;
        TypeInfo *type = NULL;
        int size = -1;

        // First, do check for "sizeof(type)".
        if (consumeReserved("(")) {
            type = parseBaseType(NULL);
            if (type) {
                Obj *obj = parseAdvancedTypeDeclaration(type, 0);
                if (obj->token)
                    errorAt(obj->token, "Unexpected ident.");
                type = obj->type;
                expectReserved(")");
            }
        }

        if (type) {
            size = sizeOf(type);
            if (size < 0)
                errorAt(tokenSave, "Incomplete type.");
        } else {
            // Then, if "sizeof(type)" didn't match the case, do check for
            // "sizeof {unary-expr}".
            globals.token = tokenSave;
            type = unary()->type;
            size = sizeOf(type);
            if (size < 0)
                errorAt(tokenSave, "Variable has incomplete type.");
        }

        n = newNodeNum(size);
    } else {
        return postfix();
    }
    n->token = tokenOperator;
    return n;
}

static Node *compoundLiteral(void) {
    Token * tokenSave = globals.token;
    TypeInfo *type = NULL;

    if (!consumeReserved("("))
        return NULL;


    type = parseBaseType(NULL);
    if (!type || type->type != TypeStruct) {
        globals.token = tokenSave;
        return NULL;
    }

    expectReserved(")");
    if (!matchReserved("{")) {
        globals.token = tokenSave;
        return NULL;
    }

    if (sizeOf(type) < 0)
        errorAt(tokenSave->next, "Incomplete type.");

    if (globals.currentEnv == &globals.globalEnv) {
        // Compound-literal at global scope.
        Node *init = NULL;
        Obj *obj = NULL;
        GVar *gvar = NULL;

        gvar = newGVar(NULL);
        init = varInitializer();

        obj = newObj(tokenSave, type, -1);
        obj->staticVarID = globals.staticVarCount++;
        obj->isStatic = 1;
        obj->token = buildTagNameForNamelessObject(obj->staticVarID);
        gvar->obj = obj;
        gvar->initializer = buildGVarInitSection(type, init);

        gvar->next = globals.staticVars;
        globals.staticVars = gvar;

        return newNodeLVar(obj);
    } else {
        // Compound-literal at local scope.
        Node *n = NULL;
        Node *init = NULL;
        Node *objNode = NULL;
        Obj *obj = NULL;

        // Size of newly captured memory for this temporal object.
        int newCap = 0;
        int offset = 0;
        int align = 0;

        newCap = sizeOf(type);

        align = alignOf(type);
        for (Env *env = globals.currentEnv; env; env = env->outer)
            offset += env->varSize;

        if (offset % align)
            newCap += align - (offset % align);

        offset += newCap;
        globals.currentEnv->varSize += newCap;
        if (globals.currentFunction->func->capStackSize < offset)
            globals.currentFunction->func->capStackSize = offset;

        obj = newObj(NULL, type, offset);
        obj->token = tokenSave;
        objNode = newNodeLVar(obj);
        init = varInitializer();

        n = newNode(NodeExprList, type);
        n->body = buildStructInitNodes(objNode, type, init);
        for (Node *c = n->body; c; c = c->next) {
            if (!c->next) {
                c->next = objNode;
                break;
            }
        }
        return n;
    }
}

static Node *postfix(void) {
    Token *tokenSave = globals.token;
    Token *ident = consumeIdent();
    Node *head = NULL;
    Node *n = NULL;

    if (ident && matchReserved("(")) {
        FCall *arg;
        Obj *f;

        if (matchToken(ident, "__builtin_va_start", 18)) {
            n = newNodeFCall(&Types.Void);
            n->kind = NodeVaStart;
            n->parentFunc = globals.currentFunction->func;
        } else {
            f = findFunction(ident->str, ident->len);
            if (!f) {
                errorAt(ident, "No such function.");
            }
            n = newNodeFCall(f->func->retType);
        }

        arg = funcArgList();

        n->fcall->name = ident->str;
        n->fcall->len = ident->len;
        n->fcall->args = arg->args;
        n->fcall->argsCount = arg->argsCount;
        n->token = ident;

        safeFree(arg);  // Do NOT free its members! They've been used!
    } else {
        globals.token = tokenSave;

        n = compoundLiteral();

        if (n) {
            while (n->next)
                n = n->next;
        } else {
            globals.token = tokenSave;
            n = primary();
        }
    }

    for (;;) {
        if (consumeReserved("[")) {
            Node *ex = NULL;
            TypeInfo *exprType = n->type;
            for (;;) {
                if (!isWorkLikePointer(exprType))
                    errorAt(globals.token->prev, "Array or pointer is needed.");
                ex = expr();
                expectReserved("]");
                n = newNodeBinary(NodeAdd, n, ex, exprType);
                exprType = exprType->baseType;
                if (!consumeReserved("["))
                    break;
            }
            n = newNodeBinary(NodeDeref, NULL, n, exprType);
        } else if (matchReserved("(")) {
            errorAt(globals.token,
                    "Calling returned function object is not supported yet.");
        } else if (consumeReserved("++")) {
            n = newNodeBinary(NodePostIncl, n, NULL, n->type);
        } else if (consumeReserved("--")) {
            n = newNodeBinary(NodePostDecl, n, NULL, n->type);
        } else if (consumeReserved(".") || consumeReserved("->")) {
            Token *memberToken;
            Obj *member;

            if (globals.token->prev->str[0] == '-') {
                if (n->type->type != TypePointer ||
                        n->type->baseType->type != TypeStruct)
                    errorAt(n->token, "Pointer for struct required.");
                n = newNodeBinary(NodeDeref, NULL, n, n->type->baseType);
            }

            if (n->type->type != TypeStruct)
                errorAt(n->token, "Struct required.");

            memberToken = expectIdent();
            member = findStructMember(n->type->structDef, memberToken->str, memberToken->len);
            if (!member)
                errorAt(memberToken, "No such struct member.");

            n = newNodeBinary(NodeMemberAccess, n, NULL, member->type);
        } else {
            break;
        }
    }

    if (head)
        return head;
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
        GVar *gvar = NULL;
        EnumItem *enumItem = NULL;
        if (lvar) {
            n = newNodeLVar(lvar);
        } else if ((gvar = findGlobalVar(ident->str, ident->len)) != NULL) {
            n = newNode(NodeGVar, gvar->obj->type);
            n->obj = gvar->obj;
        } else if ((enumItem = findEnumItem(ident->str, ident->len)) != NULL) {
            n = newNodeNum(enumItem->value);
        } else {
            errorAt(ident, "Undefined variable");
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
