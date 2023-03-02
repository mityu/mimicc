#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "mimic.h"

static void verifyTypeFCall(const Node *n);
static int checkAssignable(const TypeInfo *lhs, const TypeInfo *rhs);
static int checkComparable(const TypeInfo *t1, const TypeInfo *t2);
static int isIntegerType(const TypeInfo *t);
static int isArithmeticType(const TypeInfo *t);
static int isLvalue(const Node *n);

void verifyFlow(const Node *n) {
    static int loopDepth = 0;

    if (n == NULL)
        return;

    if (n->kind == NodeFunction) {
        verifyFlow(n->body);
    } else if (n->kind == NodeBlock) {
        for (Node *c = n->body; c; c = c->next) {
            verifyFlow(c);
        }
    } else if (n->kind == NodeFor) {
        loopDepth++;
        verifyFlow(n->body);
        loopDepth--;
        runtimeAssert(loopDepth >= 0);
    } else if (n->kind == NodeDoWhile) {
        loopDepth++;
        verifyFlow(n->body);
        loopDepth--;
        runtimeAssert(loopDepth >= 0);
    } else if (n->kind == NodeIf) {
        verifyFlow(n->body);
        for (Node *c = n->elseblock; c; c = c->next) {
            verifyFlow(c->body);
        }
    } else if (n->kind == NodeBreak || n->kind == NodeContinue) {
        if (loopDepth <= 0) {
            errorAt(n->token->str, "%s outside of loop",
                    n->kind == NodeBreak ? "break" : "continue");
        }
    }
}

void verifyType(const Node *n) {
    static Obj *currentFunction = NULL;

    if (n == NULL)
        return;

    if (n->kind == NodeFunction) {
        Obj *funcSave = currentFunction;
        currentFunction = n->func;
        verifyType(n->body);
        currentFunction = funcSave;
    } else if (n->kind == NodeBlock) {
        for (Node *c = n->body; c; c = c->next) {
            verifyType(c);
        }
    } else if (n->kind == NodeFor) {
        if (n->initializer) {
            verifyType(n->initializer);
        }
        if (n->condition) {
            verifyType(n->condition);
        }
        if (n->iterator) {
            verifyType(n->iterator);
        }
        verifyType(n->body);
    } else if (n->kind == NodeDoWhile) {
        verifyType(n->condition);
        verifyType(n->body);
    } else if (n->kind == NodeIf) {
        verifyType(n->condition);
        verifyType(n->body);
        for (Node *c = n->elseblock; c; c = c->next) {
            if (c->kind == NodeElseif)
                verifyType(c->condition);
            verifyType(c->body);
        }
    } else if (n->kind == NodeReturn) {
        // Check if {expr} is valid.
        verifyType(n->lhs);

        // Check the returned value's types.
        if (!checkAssignable(currentFunction->func->retType, n->lhs->type)) {
            errorAt(n->token->str, "Type mismatch. Cannot return this.");
        }
    } else if (n->kind == NodeDeref) {
        // Type check is needed for some cases like: v = *f(...);
        verifyType(n->rhs);
    } else if (n->kind == NodeAddress) {
        if (!isLvalue(n->rhs)) {
            errorAt(n->token->str, "Not a lvalue. Cannot take reference for this.");
        }
        // No type check is needed for lvalue nodes.
    } else if (n->kind == NodeFCall) {
        verifyTypeFCall(n);
    } else if (n->kind == NodeAssign) {
        if (!isLvalue(n->lhs)) {
            errorAt(n->token->str, "Not a lvalue. Cannot assign.");
            return;
        }
        if (!checkAssignable(n->lhs->type, n->rhs->type)) {
            errorAt(n->token->str, "Type mismatch. Cannot assign.");
            return;
        }
        verifyType(n->rhs);
    } else if (n->kind == NodeEq || n->kind == NodeNeq ||
            n->kind == NodeLE || n->kind == NodeLT) {
        verifyType(n->lhs);
        if (!checkComparable(n->lhs->type, n->rhs->type)) {
            errorAt(n->token->str, "Type mismatch. Cannot compare these.");
            return;
        }
        verifyType(n->rhs);
        return;
    } else if (n->kind == NodeAdd) {
        verifyType(n->lhs);
        if (isWorkLikePointer(n->lhs->type)) {
            if (!isIntegerType(n->rhs->type)) {
                errorAt(n->token->str, "Invalid operation for pointer.");
                return;
            }
        } else if (isWorkLikePointer(n->rhs->type)) {
            if (!isIntegerType(n->lhs->type)) {
                errorAt(n->token->str, "Invalid operation for pointer.");
                return;
            }
        } else if (!(isArithmeticType(n->lhs->type) && isArithmeticType(n->rhs->type))) {
            errorAt(n->token->str, "This operation is not supported.");
        }
        verifyType(n->rhs);
    } else if (n->kind == NodeSub) {
        verifyType(n->lhs);
        if (isWorkLikePointer(n->rhs->type)) {
            if (!isWorkLikePointer(n->lhs->type)) {
                errorAt(n->token->str, "Invalid operation for binary operator");
            }
        } else if (isWorkLikePointer(n->lhs->type)) {
            // When lhs is pointer, only pointer or integers are accepted as
            // rhs.  At here, rhs must not be pointer, so only check if rhs is
            // integer.
            if (!isIntegerType(n->rhs->type)) {
                errorAt(n->token->str, "Invalid operation for binary operator");
            }
        } else if (!(isArithmeticType(n->lhs->type) && isArithmeticType(n->rhs->type))) {
            errorAt(n->token->str, "This operation is not supported.");
        }
    } else if (n->kind == NodeMul || n->kind == NodeDiv) {
        verifyType(n->lhs);
        if (!(isArithmeticType(n->lhs->type) && isArithmeticType(n->rhs->type))) {
            errorAt(n->token->str, "This operation is not supported.");
        }
        verifyType(n->rhs);
        return;
    } else if (n->kind == NodeDivRem) {
        verifyType(n->lhs);
        if (!(isIntegerType(n->lhs->type) && isIntegerType(n->rhs->type))) {
            errorAt(n->token->str, "This operation is not supported.");
        }
        verifyType(n->rhs);
        return;
    } else if (n->kind == NodePostIncl || n->kind == NodePostDecl ||
            n->kind == NodePreIncl || n->kind == NodePreDecl) {
        Node *value =
            (n->kind == NodePreIncl || n->kind == NodePreDecl) ? n->rhs : n->lhs;
        if (!isLvalue(value)) {
            int isIncrement = n->kind == NodePreIncl || n->kind == NodePostIncl;
            errorAt(
                    value->token->str,
                    "Lvalue is required for %s operator",
                    isIncrement ? "incremental" : "decremental"
                    );
        }
        verifyType(value);
    }
}

// Check called function exists, the number of argument matches, and all the
// actual arguments are assignable to the formal parameters.  Exit program if
// some does not match.
static void verifyTypeFCall(const Node *n) {
#define ARGS_BUFFER_SIZE    (10)
    Node *actualArgBuf[ARGS_BUFFER_SIZE];
    Node **actualArgs = actualArgBuf;
    Node *arg = NULL;
    Obj *formalArg = NULL;

    if (n->kind != NodeFCall) {
        error("Internal error: Not a NodeFCall node is given.");
    }
    Obj *f = findFunction(n->fcall->name, n->fcall->len);
    if (!f) {
        errorUnreachable();
    }
    if (n->fcall->argsCount != f->func->argsCount && !f->func->haveVaArgs) {
        errorAt(
                n->token->str,
                "%d arguments are expected, but got %d.",
                f->func->argsCount,
                n->fcall->argsCount
                );
    } else if (n->fcall->argsCount < f->func->argsCount && f->func->haveVaArgs) {
        errorAt(
                n->token->str,
                "At least %d arguments are expected, but got just %d.",
                f->func->argsCount,
                n->fcall->argsCount
                );
    }

    if (f->func->argsCount > ARGS_BUFFER_SIZE) {
        actualArgs =
            (Node **)safeAlloc(f->func->argsCount * sizeof(Node *));
    }

    if (f->func->argsCount == 0)
        return;

    arg = n->fcall->args;
    // Skip args in variadic arguments area
    for (int i = 0; i < n->fcall->argsCount - f->func->argsCount; ++i)
        arg = arg->next;
    for (Node **store = &actualArgs[f->func->argsCount-1]; arg; arg = arg->next) {
        *store = arg;
        --store;
    }

    formalArg = f->func->args;
    for (int i = 0; i < f->func->argsCount; ++i) {
        if (!checkAssignable(formalArg->type, actualArgs[i]->type)) {
            errorAt(
                    actualArgs[i]->token->str,
                    "Type mismatch."
                    );
        }
        formalArg = formalArg->next;
    }
    if (f->func->argsCount > ARGS_BUFFER_SIZE) {
        free(actualArgs);
        actualArgs = NULL;
    }
#undef ARGS_BUFFER_SIZE
}

// Check if rhs is assignable to lhs.  Return TRUE if can.
static int checkAssignable(const TypeInfo *lhs, const TypeInfo *rhs) {
    if (lhs->type == TypePointer) {
        TypeInfo *t;
        // void* accepts any pointer/array type.
        for (t = lhs->baseType; t->type == TypePointer; t = t->baseType);
        if (t->type == TypeVoid)
            return isWorkLikePointer(rhs);

        // void* can be assigned to any pointer type.
        if (rhs->type == TypePointer) {
            for (t = rhs->baseType; t->type == TypePointer; t = t->baseType);
            if (t->type == TypeVoid)
                return 1;
        }

        if (isWorkLikePointer(rhs)) {
            return checkAssignable(lhs->baseType, rhs->baseType);
        }
        return 0;
    } else if (lhs->type == TypeArray) {
        return 0;
    } else if (rhs->type == TypeNumber) {
        return lhs->type == TypeInt || lhs->type == TypeChar || lhs->type == TypeEnum;
    } else if (lhs->type == TypeInt) {
        return rhs->type == TypeInt || rhs->type == TypeChar || rhs->type == TypeEnum;  // TODO: Truely OK?
    } else {
        return lhs->type == rhs->type;
    }
    errorUnreachable();
}

static int checkComparable(const TypeInfo *t1, const TypeInfo *t2) {
    // What's real type?
    if (isArithmeticType(t1) && isArithmeticType(t2)) {
        return 1;
    } else if (isWorkLikePointer(t1)) {
        TypeInfo *t;
        for (t = t1->baseType; t->type == TypePointer; t = t->baseType);
        if (t->type == TypeVoid)
            return isWorkLikePointer(t2);

        for (t = t2->baseType; t->type == TypePointer; t = t->baseType);
        if (t->type == TypeVoid)
            return isWorkLikePointer(t1);

        if (isWorkLikePointer(t2)) {
            return checkComparable(t1->baseType, t2->baseType);
        }
        return 0;
    }
    return 0;
}

int checkTypeEqual(const TypeInfo *t1, const TypeInfo *t2) {
    if (t1->type != t2->type) {
        return 0;
    } else if (t1->type == TypePointer || t1->type == TypeArray) {
        return checkTypeEqual(t1->baseType, t2->baseType);
    } else if (t1->type == TypeFunction) {
        TypeInfo *arg1, *arg2;
        if (!checkTypeEqual(t1->retType, t2->retType))
            return 0;

        arg1 = t1->argTypes;
        arg2 = t2->argTypes;
        for (; arg1; arg1 = arg1->next, arg2 = arg2->next) {
            if (!(arg2 && checkTypeEqual(arg1, arg2)))
                return 0;
        }
        if (arg2)
            return 0;
    } else if (t1->type == TypeStruct) {
        // TODO: Implement
    }
    return 1;
}

// Return TRUE if given type is integer type.
static int isIntegerType(const TypeInfo *t) {
    return t->type == TypeInt || t->type == TypeNumber || t->type == TypeEnum;
}

// Return TRUE if given type is arithmetic type.
static int isArithmeticType(const TypeInfo *t) {
    return t->type == TypeChar || t->type == TypeInt || t->type == TypeNumber || t->type == TypeEnum;
}

// Return TRUE is `n` is lvalue.
static int isLvalue(const Node *n) {
    return n->kind == NodeLVar || n->kind == NodeGVar ||
        n->kind == NodeDeref || n->kind == NodeMemberAccess;
}

// Return TRUE if given type can work like a pointer. Currently returns TRUE
// when given type is pointer or array.
int isWorkLikePointer(const TypeInfo *t) {
    if (t->type == TypePointer || t->type == TypeArray)
        return 1;
    return 0;
}
