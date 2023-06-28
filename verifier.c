#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "mimicc.h"

static void verifyTypeFCall(const Node *n);
static int checkRelationallyComparable(const TypeInfo *t1, const TypeInfo *t2);
static int isIntegerType(const TypeInfo *t);
static int isArithmeticType(const TypeInfo *t);
static int isScalarType(const TypeInfo *t);

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
    } else if (n->kind == NodeSwitch) {
        loopDepth++;
        verifyFlow(n->body);
        loopDepth--;
        runtimeAssert(loopDepth >= 0);
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
            errorAt(n->token, "%s outside of loop",
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
        currentFunction = n->obj;
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
    } else if (n->kind == NodeExprList) {
        verifyType(n->body);
    } else if (n->kind == NodeConditional) {
        // TODO: Check type of n->lhs and n->rhs matches or not.
        // For the details, see P.90 on C11 draft.
    } else if (n->kind == NodeIf) {
        verifyType(n->condition);
        verifyType(n->body);
        for (Node *c = n->elseblock; c; c = c->next) {
            if (c->kind == NodeElseif)
                verifyType(c->condition);
            verifyType(c->body);
        }
    } else if (n->kind == NodeReturn) {
        int isReturnable = 0;
        int isBothVoid = 0;

        // Check if {expr} is valid.
        verifyType(n->lhs);

        // Check the returned value's types.
        isReturnable = checkAssignable(currentFunction->func->retType, n->type);
        isBothVoid =
            currentFunction->func->retType->type == TypeVoid &&
            n->type->type == TypeVoid;
        if (!(isReturnable || isBothVoid)) {
            errorAt(n->token, "Type mismatch. Cannot return this.");
        }
    } else if (n->kind == NodeDeref) {
        // Type check is needed for some cases like: v = *f(...);
        verifyType(n->rhs);
    } else if (n->kind == NodeAddress) {
        if (!isLvalue(n->rhs)) {
            errorAt(n->token, "Not a lvalue. Cannot take reference for this.");
        }
        // No type check is needed for lvalue nodes.
    } else if (n->kind == NodeFCall) {
        verifyTypeFCall(n);
    } else if (n->kind == NodeAssign) {
        if (!isLvalue(n->lhs))
            errorAt(n->token, "Not a lvalue. Cannot assign.");

        if (!checkAssignable(n->lhs->type, n->rhs->type))
            errorAt(n->token, "Type mismatch. Cannot assign.");

        verifyType(n->rhs);
    } else if (n->kind == NodeAssignStruct) {
        if (n->lhs->type->type != TypeStruct)
            errorUnreachable();
        else if (n->rhs->type->type != TypeStruct)
            errorAt(n->rhs->token, "Struct required.");
        else if (n->lhs->type->structDef != n->rhs->type->structDef)
            errorAt(n->token, "Different struct.");

        verifyType(n->rhs);
    } else if (n->kind == NodeEq || n->kind == NodeNeq) {
        verifyType(n->lhs);
        if (!checkRelationallyComparable(n->lhs->type, n->rhs->type)) {
            // Different from "<" and "<=", "==" and "!=" are allowed to
            // compare pointer with NULL constant, so do check for it.
            Node *other = NULL;

            if (isWorkLikePointer(n->lhs->type))
                other = evalConstantExpr(n->rhs);
            else if (isWorkLikePointer(n->rhs->type))
                other = evalConstantExpr(n->lhs);

            if (!(other && other->kind == NodeNum && other->val == 0))
                errorAt(n->token, "Type mismatch. Cannot compare these.");
        }
        verifyType(n->rhs);
    } else if (n->kind == NodeLE || n->kind == NodeLT) {
        verifyType(n->lhs);
        if (!checkRelationallyComparable(n->lhs->type, n->rhs->type)) {
            errorAt(n->token, "Type mismatch. Cannot compare these.");
        }
        verifyType(n->rhs);
    } else if (n->kind == NodeAdd) {
        verifyType(n->lhs);
        if (isWorkLikePointer(n->lhs->type)) {
            if (!isIntegerType(n->rhs->type)) {
                errorAt(n->token, "Invalid operation for pointer.");
            }
        } else if (isWorkLikePointer(n->rhs->type)) {
            if (!isIntegerType(n->lhs->type)) {
                errorAt(n->token, "Invalid operation for pointer.");
            }
        } else if (!(isArithmeticType(n->lhs->type) && isArithmeticType(n->rhs->type))) {
            errorAt(n->token, "This operation is not supported.");
        }
        verifyType(n->rhs);
    } else if (n->kind == NodeSub) {
        verifyType(n->lhs);
        if (isWorkLikePointer(n->rhs->type)) {
            if (!isWorkLikePointer(n->lhs->type)) {
                errorAt(n->token, "Invalid operation for binary operator");
            }
        } else if (isWorkLikePointer(n->lhs->type)) {
            // When lhs is pointer, only pointer or integers are accepted as
            // rhs.  At here, rhs must not be pointer, so only check if rhs is
            // integer.
            if (!isIntegerType(n->rhs->type)) {
                errorAt(n->token, "Invalid operation for binary operator");
            }
        } else if (!(isArithmeticType(n->lhs->type) && isArithmeticType(n->rhs->type))) {
            errorAt(n->token, "This operation is not supported.");
        }
    } else if (n->kind == NodeMul || n->kind == NodeDiv) {
        verifyType(n->lhs);
        if (!(isArithmeticType(n->lhs->type) && isArithmeticType(n->rhs->type))) {
            errorAt(n->token, "This operation is not supported.");
        }
        verifyType(n->rhs);
    } else if (n->kind == NodeDivRem) {
        verifyType(n->lhs);
        if (!(isIntegerType(n->lhs->type) && isIntegerType(n->rhs->type))) {
            errorAt(n->token, "This operation is not supported.");
        }
        verifyType(n->rhs);
    } else if (n->kind == NodeLogicalAND || n->kind == NodeLogicalOR) {
        verifyType(n->lhs);
        if (!(isScalarType(n->lhs->type) && isScalarType(n->rhs->type))) {
            errorAt(n->token, "Both operands should have scalar type.");
        }
        verifyType(n->rhs);
    } else if (n->kind == NodePostIncl || n->kind == NodePostDecl ||
            n->kind == NodePreIncl || n->kind == NodePreDecl) {
        Node *value =
            (n->kind == NodePreIncl || n->kind == NodePreDecl) ? n->rhs : n->lhs;
        if (!isLvalue(value)) {
            int isIncrement = n->kind == NodePreIncl || n->kind == NodePostIncl;
            errorAt(
                    value->token,
                    "Lvalue is required for %s operator",
                    isIncrement ? "incremental" : "decremental"
                    );
        }
        verifyType(value);
    } else if (n->kind == NodeBitwiseAND ||n->kind == NodeBitwiseOR ||
            n->kind == NodeBitwiseXOR) {
        verifyType(n->lhs);
        if (!(isIntegerType(n->lhs->type) && isIntegerType(n->rhs->type)))
            errorAt(n->token, "Both operands should have integer type.");
        verifyType(n->rhs);
    } else if (n->kind == NodeNot) {
        if (!isScalarType(n->rhs->type))
            errorAt(n->token, "Operand should has scalar type.");
        verifyType(n->rhs);
    } else if (n->kind == NodeTypeCast) {
        // TODO: Check n->rhs lefts a value (?)
        // TODO: Check cast operation can be carried out.
        verifyType(n->rhs);
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
    Function *fdecl = NULL;

    if (n->kind != NodeFCall) {
        error("Internal error: Not a NodeFCall node is given.");
    }

    for (TypeInfo *base = n->fcall->declType;;) {
        if (base->type == TypeFunction) {
            fdecl = base->funcDef;
            break;
        } else if (base->type == TypePointer) {
            base = base->baseType;
        } else {
            errorUnreachable();
        }
    }

    if (n->fcall->argsCount != fdecl->argsCount && !fdecl->haveVaArgs) {
        errorAt(
                n->token,
                "%d arguments are expected, but got %d.",
                fdecl->argsCount,
                n->fcall->argsCount
                );
    } else if (n->fcall->argsCount < fdecl->argsCount && fdecl->haveVaArgs) {
        errorAt(
                n->token,
                "At least %d arguments are expected, but got just %d.",
                fdecl->argsCount,
                n->fcall->argsCount
                );
    }

    if (fdecl->argsCount > ARGS_BUFFER_SIZE) {
        actualArgs =
            (Node **)safeAlloc(fdecl->argsCount * sizeof(Node *));
    }

    if (fdecl->argsCount == 0)
        return;

    arg = n->fcall->args;
    // Skip args in variadic arguments area
    for (int i = 0; i < n->fcall->argsCount - fdecl->argsCount; ++i)
        arg = arg->next;
    for (Node **store = &actualArgs[fdecl->argsCount-1]; arg; arg = arg->next) {
        *store = arg;
        --store;
    }

    formalArg = fdecl->args;
    for (int i = 0; i < fdecl->argsCount; ++i) {
        if (!checkAssignable(formalArg->type, actualArgs[i]->type)) {
            errorAt(
                    actualArgs[i]->token,
                    "Type mismatch."
                    );
        }
        formalArg = formalArg->next;
    }
    if (fdecl->argsCount > ARGS_BUFFER_SIZE) {
        free(actualArgs);
        actualArgs = NULL;
    }
#undef ARGS_BUFFER_SIZE
}

// Check if rhs is assignable to lhs.  Return TRUE if can.
int checkAssignable(const TypeInfo *lhs, const TypeInfo *rhs) {
    if (isArithmeticType(lhs)) {
        return isArithmeticType(rhs);
    } else if (lhs->type == TypeStruct) {
        return checkTypeEqual(lhs, rhs);
    } else if (lhs->type == TypePointer &&
            getBaseType(lhs)->type == TypeFunction) {
        return checkTypeEqual(getBaseType(lhs), getBaseType(rhs));

    } else if (lhs->type == TypePointer && isWorkLikePointer(rhs)) {
        const TypeInfo *lhsBase = getBaseType(lhs);
        if (lhsBase->type == TypeVoid || getBaseType(rhs)->type == TypeVoid) {
            return 1;
        }
        return checkTypeEqual(lhs->baseType, rhs->baseType);
    } else if (lhs->type == TypePointer) {
        if (rhs->type == TypeNumber) {
            return 0;  // TODO: Allow when assigning NULL.
        }
        return 0;
    }
    return 0;
}

// Return TRUE if operands are compareble.
static int checkRelationallyComparable(const TypeInfo *t1, const TypeInfo *t2) {
    if (isArithmeticType(t1) && isArithmeticType(t2)) {
        return 1;
    } else if (isWorkLikePointer(t1) && isWorkLikePointer(t2)) {
        const TypeInfo *base;

        base = t1->baseType;
        while (base->type == TypePointer)
            base = base->baseType;
        if (base->type == TypeVoid)
            return 1;

        base = t2->baseType;
        while (base->type == TypePointer)
            base = base->baseType;
        if (base->type == TypeVoid)
            return 1;

        return checkRelationallyComparable(t1->baseType, t2->baseType) ||
            checkTypeEqual(t1->baseType, t2->baseType);
    }
    return 0;
}

int checkTypeEqual(const TypeInfo *t1, const TypeInfo *t2) {
    if (t1->type != t2->type) {
        return 0;
    } else if (t1->type == TypePointer || t1->type == TypeArray) {
        return checkTypeEqual(t1->baseType, t2->baseType);
    } else if (t1->type == TypeFunction) {
        Obj *arg1, *arg2;
        if (!checkTypeEqual(t1->funcDef->retType, t2->funcDef->retType))
            return 0;

        arg1 = t1->funcDef->args;
        arg2 = t2->funcDef->args;
        for (; arg1; arg1 = arg1->next, arg2 = arg2->next) {
            if (!(arg2 && checkTypeEqual(arg1->type, arg2->type)))
                return 0;
        }
        if (arg2)
            return 0;
    } else if (t1->type == TypeStruct) {
        return t1->structDef == t2->structDef;
    } else if (t1->type == TypeEnum) {
        return t1->enumDef == t2->enumDef;
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

static int isScalarType(const TypeInfo *t) {
    return isArithmeticType(t) || isWorkLikePointer(t);
}

// Return TRUE is `n` is lvalue.
int isLvalue(const Node *n) {
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

// Get base type of pointer/array.
const TypeInfo *getBaseType(const TypeInfo *type) {
    while (type->type == TypePointer || type->type == TypeArray)
        type = type->baseType;
    return type;
}
