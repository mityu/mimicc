#include <stddef.h>
#include <stdio.h>
#include "mimic.h"

static void verifyTypeFCall(Node *n);
static int checkAssignable(TypeInfo *lhs, TypeInfo *rhs);
static int checkComparable(TypeInfo *t1, TypeInfo *t2);
static int isLvalue(Node *n);

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
    } else if (n->kind == NodeAdd || n->kind == NodeSub) {
        verifyType(n->lhs);
        if (n->lhs->type->type == TypePointer) {
            TypeKind t = n->rhs->type->type;
            if (!(t == TypeInt || t == TypeNumber)) {
                errorAt(n->token->str, "Invalid operation for pointer.");
                return;
            }
        } else if (n->lhs->type->type == TypeInt || n->lhs->type->type == TypeNumber) {
            TypeKind t = n->rhs->type->type;
            if (!(t == TypeInt || t == TypeNumber)) {
                errorAt(n->token->str, "Type mismatch. Cannot do this operation.");
                return;
            }
        } else {
            errorAt(n->token->str, "This operation is not supported.");
        }
        verifyType(n->rhs);
    } else if (n->kind == NodeMul || n->kind == NodeDiv || n->kind == NodeDivRem) {
        verifyType(n->lhs);
        if (n->lhs->type->type == TypeInt || n->lhs->type->type == TypeNumber) {
            if (!(n->rhs->type->type == TypeInt || n->rhs->type->type == TypeNumber)) {
                errorAt(n->token->str, "Type mismatch. Cannot do this operation.");
                return;
            }
        } else {
            errorAt(n->token->str, "This operation is not supported.");
        }
        verifyType(n->rhs);
        return;
    }
}

// Check called function exists, the number of argument matches, and all the
// actual arguments are assignable to the formal parameters.  Exit program if
// some does not match.
static void verifyTypeFCall(Node *n) {
    LVar *formalArg = NULL;
    Node *actualArg = NULL;

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

    formalArg = f->args;
    actualArg = n->fcall->args;
    for (int i = 0; i < f->argsCount; ++i) {
        if (!checkAssignable(formalArg->type, actualArg->type)) {
            errorAt(
                    actualArg->token->str,
                    "Type mismatch."
                    );
        }
        formalArg = formalArg->next;
        actualArg = actualArg->next;
    }
}

// Check if rhs is assignable to lhs.  Return TRUE if can.
static int checkAssignable(TypeInfo *lhs, TypeInfo *rhs) {
    if (lhs->type == TypePointer) {
        if (rhs->type == TypePointer) {
            return checkAssignable(lhs->ptrTo, rhs->ptrTo);
        }
        return 0;
    } else if (rhs->type == TypeNumber) {
        return lhs->type == TypeInt;
    } else {
        return lhs->type == rhs->type;
    }
}

static int checkComparable(TypeInfo *t1, TypeInfo *t2) {
    if (t1->type == TypeNumber || t2->type == TypeNumber) {
        TypeInfo *t = t2;
        if (t2->type == TypeNumber)
            t = t1;
        return t->type == TypeNumber || t->type == TypeInt;
    } else if (t1->type == TypePointer) {
        if (t2->type == TypePointer)
            return checkComparable(t1->ptrTo, t2->ptrTo);
        return 0;
    } else {
        return t1->type == t2->type;
    }
}

// Return TRUE is `n` is lvalue.
static int isLvalue(Node *n) {
    return n->kind == NodeLVar || n->kind == NodeDeref;
}
