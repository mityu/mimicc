#include <stddef.h>
#include <stdio.h>
#include "mimic.h"

static void verifyTypeFCall(Node *n);
static int checkAssignable(TypeInfo *lhs, TypeInfo *rhs);
static int checkTypeMatches(TypeInfo *t1, TypeInfo *t2);

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
        verifyType(n->rhs);  // TODO: This is really necessary?
    } else if (n->kind == NodeFCall) {
        verifyTypeFCall(n);
    } else if (n->kind == NodeAssign) {
        if (!(n->lhs->kind == NodeLVar || n->lhs->kind == NodeDeref)) {
            errorAt(n->token->str, "Not a lvalue. Cannot assign.");
            return;
        }
        if (!checkAssignable(n->lhs->type, n->rhs->type)) {
            errorAt(n->token->str, "Type mismatch. Cannot assign.");
            return;
        }
    } else if (n->kind == NodeEq || n->kind == NodeNeq ||
            n->kind == NodeLE || n->kind == NodeLT) {
        // TODO:
        return;
    } else if (n->kind == NodeAdd || n->kind == NodeSub ||
            n->kind == NodeMul || n->kind == NodeDiv || n->kind == NodeDivRem) {
        // TODO:
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

static int checkTypeMatches(TypeInfo *t1, TypeInfo *t2) {
    return 1;
}
