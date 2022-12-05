#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "mimic.h"

static void verifyTypeFCall(Node *n);
static int checkAssignable(TypeInfo *lhs, TypeInfo *rhs);
static int checkComparable(TypeInfo *t1, TypeInfo *t2);
static int checkTypeEqual(TypeInfo *t1, TypeInfo *t2);
static int isIntegerType(TypeInfo *t);
static int isArithmeticType(TypeInfo *t);
static int isLvalue(Node *n);
static int isWorkLikePointer(TypeInfo *t);

void verifyType(Node *n) {
    if (n->kind == NodeFunction) {
        verifyType(n->body);
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
    } else if (n->kind == NodeIf) {
        verifyType(n->condition);
        verifyType(n->body);
        for (Node *c = n->elseblock; c; c = c->next) {
            if (c->kind == NodeElseif)
                verifyType(n->condition);
            verifyType(n->body);
        }
    } else if (n->kind == NodeReturn) {
        Node *funcNode = NULL;

        // Check if {expr} is valid.
        verifyType(n->lhs);

        // Check the returned value's types.
        funcNode = n->outerBlock;
        for (; funcNode; funcNode = funcNode->outerBlock) {
            if (funcNode->kind == NodeFunction) {
                break;
            }
        }
        if (!funcNode) {
            errorUnreachable();
        }
        if (!checkAssignable(funcNode->func->retType, n->lhs->type)) {
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
static void verifyTypeFCall(Node *n) {
#define ARGS_BUFFER_SIZE    (10)
    Node *actualArgBuf[ARGS_BUFFER_SIZE];
    Node **actualArgs = actualArgBuf;
    LVar *formalArg = NULL;

    if (n->kind != NodeFCall) {
        error("Internal error: Not a NodeFCall node is given.");
        return;
    }
    Function *f = findFunction(n->fcall->name, n->fcall->len);
    if (!f) {
        errorUnreachable();
    }
    if (n->fcall->argsCount != f->argsCount) {
        errorAt(
                n->token->str,
                "%d arguments are expected, but got %d.",
                f->argsCount,
                n->fcall->argsCount
                );
    }

    if (f->argsCount > ARGS_BUFFER_SIZE) {
        actualArgs =
            (Node **)calloc(f->argsCount, sizeof(Node *));
    }
    for (Node *c = n->fcall->args, **store = &actualArgs[f->argsCount-1]; c; c = c->next) {
        *store = c;
        --store;
    }

    formalArg = f->args;
    for (int i = 0; i < f->argsCount; ++i) {
        if (!checkAssignable(formalArg->type, actualArgs[i]->type)) {
            errorAt(
                    actualArgs[i]->token->str,
                    "Type mismatch."
                    );
        }
        formalArg = formalArg->next;
    }
    if (f->argsCount > ARGS_BUFFER_SIZE) {
        free(actualArgs);
        actualArgs = NULL;
    }
#undef ARGS_BUFFER_SIZE
}

// Check if rhs is assignable to lhs.  Return TRUE if can.
static int checkAssignable(TypeInfo *lhs, TypeInfo *rhs) {
    if (isWorkLikePointer(lhs)) {
        if (isWorkLikePointer(rhs)) {
            return checkAssignable(lhs->baseType, rhs->baseType);
        }
        return 0;
    } else if (rhs->type == TypeNumber) {
        return lhs->type == TypeInt;
    } else {
        return lhs->type == rhs->type;
    }
}

static int checkComparable(TypeInfo *t1, TypeInfo *t2) {
    // What's real type?
    if (isArithmeticType(t1) && isArithmeticType(t2)) {
        return 1;
    } else if (isWorkLikePointer(t1)) {
        if (isWorkLikePointer(t2))
            return checkComparable(t1->baseType, t2->baseType);
        return 0;
    }
    return 0;
}

// TODO: Support array
static int checkTypeEqual(TypeInfo *t1, TypeInfo *t2) {
    if (t1->type != t2->type) {
        return 0;
    } else if (t1->type == TypePointer) {
        return checkTypeEqual(t1->baseType, t2->baseType);
    }
    return 1;
}

// Return TRUE if given type is integer type.
static int isIntegerType(TypeInfo *t) {
    return t->type == TypeInt || t->type == TypeNumber;
}

// Return TRUE if given type is arithmetic type.
static int isArithmeticType(TypeInfo *t) {
    return t->type == TypeInt || t->type == TypeNumber;
}

// Return TRUE is `n` is lvalue.
static int isLvalue(Node *n) {
    return n->kind == NodeLVar || n->kind == NodeGVar || n->kind == NodeDeref;
}

// Return TRUE if given type can work like a pointer. Currently returns TRUE
// when given type is pointer or array.
static int isWorkLikePointer(TypeInfo *t) {
    if (t->type == TypePointer || t->type == TypeArray)
        return 1;
    return 0;
}
