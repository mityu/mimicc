#include "mimicc.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// TODO: Do not use "push" and "pop" when store too much arguments to stack?
// TODO: Maybe even when sizeOf(number) returns 4, it actually uses 8 bytes.

// "push" and "pop" operator implicitly uses rsp as memory address.
// Therefore, `push rax` is equal to:
//    sub rsp, 8
//    mov [rsp], rax
// and `pop rax` is equal to:
//    mov rax, [rsp]
//    add rsp, 8
//
// In addition, rsp points to the top of the stack, "push" and "pop" works as
// stack operator.
//
// Equivalents for some control statements:
//  - if statement
//      if (A)
//        B
//
//          if (A == 0)
//             goto Lend;
//             B
//      Lend:
//
//  - if-else statement
//      if (A) B else C
//
//          if (A == 0)
//              goto Lelse;
//              B
//              goto Lend;
//      Lelse:
//          C
//      Lend:
//
//  - if-elseiflse statement
//      if (A)
//          B
//      else if (C)
//          D
//
//          if (A == 0)
//              goto Lelse;
//              B
//              goto Lend;
//      Lelse:
//          if (C == 0)
//              goto Lend;
//              D
//      Lend:
//
//  - if-elseif-else statement
//      if (A)
//          B
//      else if (C)
//          D
//      else
//          E
//
//          if (A == 0)
//              goto Lelse1;
//              B
//              goto Lend;
//      Lelse1:
//          if (C == 0)
//              goto Lelse2;
//              D
//              goto Lend;
//      Lelse2:
//          E
//      Lend:
//
//  Stack state at the head of function:
//  (When function has 8 arguments)
//
//   |                                            |
//   |                                            |
//   +--------------------------------------------+
//   |               8th argument                 |
//   +--------------------------------------------+
//   |               7th argument                 |
//   +--------------------------------------------+
//   |    Return address of previous function     |
//   +--------------------------------------------+
//   |            Saved RBP value                 | <-- RBP
//   +--------------------------------------------+
//   |               1st argument                 |
//   +--------------------------------------------+
//   |               2nd argument                 |
//   +--------------------------------------------+
//   |                                            |
//                         .
//                         .
//                         .
//   |                                            |
//   +--------------------------------------------+
//   |               6th argument                 |
//   +--------------------------------------------+
//   |                                            |
//                         .
//                         .    (Local variables or tmp values exprs left)
//                         .

typedef struct {
    Obj *currentFunc;
    int loopBlockID;
} DumpEnv;

typedef struct {
    AsmInst head;
    AsmInst *tail;
} AsmInstList;

static DumpEnv dumpEnv;
static AsmInst *genCodeInitVarArray(const Node *n, TypeInfo *varType);
static AsmInst *genCodeInitVarStruct(const Node *n, TypeInfo *varType);
static AsmInst *genCodeInitVar(const Node *n, TypeInfo *varType);

static int isEqualRegister(const Register *a, const Register *b) {
    return a->kind == b->kind && a->size == b->size;
}

static int isEqualRegisterOperand(const AsmInstOperand *a, const AsmInstOperand *b) {
    return a->mode == AsmAddressingModeRegister && a->mode == AsmAddressingModeRegister &&
           isEqualRegister(&a->src.reg, &b->src.reg);
}

static OperandSize getOperandSizeFromByteSize(int size) {
    switch (size) {
    case 1:
        return OpSize8;
    case 2:
        return OpSize16;
    case 4:
        return OpSize32;
    case 8:
        return OpSize64;
    default:
        errorUnreachable();
    }
}

static AsmInst *newAsmInst(AsmInstKind kind) {
    static AsmInst zero = {};
    AsmInst *inst = safeAlloc(sizeof(AsmInst));

    *inst = zero;
    inst->kind = kind;
    inst->next = NULL;
    inst->text = NULL;
    return inst;
}

static AsmInst *newAsmInstMov(AsmInstDataMov *mov) {
    AsmInst *inst = newAsmInst(AsmMov);
    inst->data.mov = *mov;
    return inst;
}

static AsmInst *newAsmInstAnyText(char *text) {
    AsmInst *inst = newAsmInst(AsmAnyText);
    inst->text = text;
    return inst;
}

static void freeAsmInst(AsmInst *inst) {
    switch (inst->kind) {
    case AsmAnyText:
        // TODO: We cannot apply free() for inst->text now since it may be
        // statically stored string.
        // safeFree(inst->text);
        break;
    default:
        break;
    }
    safeFree(inst);
}

/**
 * Get the last AsmInst element of a given list of AsmInst.
 */
static AsmInst *getLastAsmInst(AsmInst *inst) {
    for (; inst->next; inst = inst->next)
        ;
    return inst;
}

static void initAsmInstList(AsmInstList *list) {
    static AsmInstList zero = {};
    *list = zero;
    list->tail = &list->head;
}

/**
 * Append given AsmInst object at the end of AsmInst-list.
 * Do nothing when the given AsmInst object is NULL.
 */
static void appendAsmInst(AsmInstList *list, AsmInst *inst) {
    if (!inst) {
        return;
    } else if (list->tail->next) {
        error("Internal error: Given asm list already has next element.");
    }
    list->tail->next = inst;
    list->tail = getLastAsmInst(list->tail);
}

/**
 * Append new AsmPush-typed instruction after `list`.
 */
static void appendAsmInstPush(AsmInstList *list, AsmInstOperand *operand) {
    AsmInst *inst = newAsmInst(AsmPush);
    inst->data.push = *operand;
    appendAsmInst(list, inst);
}

/**
 * Append new AsmPop-typed instruction after `list`.
 */
static void appendAsmInstPop(AsmInstList *list, Register *r) {
    AsmInst *inst = newAsmInst(AsmPop);
    inst->data.pop = *r;
    appendAsmInst(list, inst);
}

static void appendAsmInstMov(AsmInstList *list, AsmInstDataMov *mov) {
    appendAsmInst(list, newAsmInstMov(mov));
}

/**
 * Append new AsmAnyText-typed instruction after `list`.
 */
static void appendAsmInstAnyText(AsmInstList *list, const char *fmt, ...) {
    va_list ap;
    char *text;
    AsmInst *newInst;

    va_start(ap, fmt);
    text = vformat(fmt, ap);
    va_end(ap);

    newInst = newAsmInstAnyText(text);
    appendAsmInst(list, newInst);
}

static AsmInst *getRawAsmInstList(AsmInstList *list) { return list->head.next; }

static int isExprNode(const Node *n) {
    // All cases in this switch uses fallthrough.
    switch (n->kind) {
    case NodeAdd:
    case NodeSub:
    case NodeMul:
    case NodeDiv:
    case NodeDivRem:
    case NodeEq:
    case NodeNeq:
    case NodeLT:
    case NodeLE:
    case NodePreIncl:
    case NodePreDecl:
    case NodePostIncl:
    case NodePostDecl:
    case NodeMemberAccess:
    case NodeAddress:
    case NodeDeref:
    case NodeNot:
    case NodeLogicalAND:
    case NodeLogicalOR:
    case NodeNum:
    case NodeBitwiseAND:
    case NodeBitwiseOR:
    case NodeBitwiseXOR:
    case NodeArithShiftL:
    case NodeArithShiftR:
    case NodeLiteralString:
    case NodeLVar:
    case NodeAssign:
    case NodeAssignStruct:
    case NodeAssignUnion:
    case NodeFCall:
    case NodeExprList:
    case NodeGVar:
    case NodeTypeCast:
        return 1;
    case NodeIf:
    case NodeElseif:
    case NodeElse:
    case NodeSwitch:
    case NodeSwitchCase:
    case NodeFor:
    case NodeDoWhile:
    case NodeBlock:
    case NodeBreak:
    case NodeContinue:
    case NodeReturn:
    case NodeFunction:
    case NodeClearStack:
    case NodeVaStart:
    case NodeNop:
    case NodeInitVar:
        return 0;
    case NodeConditional:
        if (n->lhs->type->type == TypeVoid)
            return 0;
        return 1;
    case NodeInitList:
        errorUnreachable();
    }
    errorUnreachable();
    return 0;
}

/**
 * Check whether given value can be stored on a register using its type
 * information, and returns TRUE if so.
 */
static int isRegisterStorableValue(const Node *n) {
    switch (n->type->type) {
    case TypeArray:
    case TypeStruct:
    case TypeUnion:
    case TypeFunction:
        return 0;
    case TypeInt:
    case TypeChar:
    case TypeNumber:
    case TypePointer:
    case TypeEnum:
        return 1;
    case TypeNone:
    case TypeVoid:
        errorUnreachable();
    }
}

static void fillNodeNum(Node *n, int val) {
    n->kind = NodeNum;
    n->val = val;
    n->type = &Types.Int;
}

static void fillNodeAdd(Node *n, Node *var, Node *shifter, TypeInfo *type) {
    n->kind = NodeAdd;
    n->lhs = var;
    n->rhs = shifter;
    n->type = type;
}

static void fillNodeInitVar(Node *n, Token *token, Node *var, Node *init) {
    n->kind = NodeInitVar;
    n->token = token;
    n->lhs = var;
    n->rhs = init;
    n->type = &Types.Void;
}

// Return an integer corresponding to 1 for given type.
// In concrate explanation, return
// - sizeof(type) for "type *" and "type[]"
// - 1 for integer.
static int getAlternativeOfOneForType(const TypeInfo *ti) {
    if (ti->type == TypePointer || ti->type == TypeArray) {
        return sizeOf(ti->baseType);
    }
    return 1;
}

// clang-format off
static const char *RegTable[RegCount][4] = {
    {"rax", "eax",  "ax",   "al"},
    {"rdi", "edi",  "di",   "dil"},
    {"rsi", "esi",  "si",   "sil"},
    {"rdx", "edx",  "dx",   "dl"},
    {"rcx", "ecx",  "cx",   "cl"},
    {"rbp", "ebp",  "bp",   "bpl"},
    {"rsp", "esp",  "sp",   "spl"},
    {"rbx", "ebx",  "bx",   "bl"},
    {"r8",  "r8d",  "r8w",  "r8b"},
    {"r9",  "r9d",  "r9w",  "r9b"},
    {"r10", "r10d", "r10w", "r10b"},
    {"r11", "r11d", "r11w", "r11b"},
    {"r12", "r12d", "r12w", "r12b"},
    {"r13", "r13d", "r13w", "r13b"},
    {"r14", "r14d", "r14w", "r14b"},
    {"r15", "r15d", "r15w", "r15b"},
};
// clang-format on

static const char *getReg(RegKind reg, int size) {
    switch (size) {
    case 1:
        return RegTable[reg][3];
    case 2:
        return RegTable[reg][2];
    case 4:
        return RegTable[reg][1];
    case 8:
        return RegTable[reg][0];
    }
    errorUnreachable();
    return "";
}

/*
static const char *argRegs[REG_ARGS_MAX_COUNT] = {
    "rdi", "rsi", "rdx", "rcx", "r8", "r9"
};
*/

static const RegKind argRegs[REG_ARGS_MAX_COUNT] = {RDI, RSI, RDX, RCX, R8, R9};

#define regobj(kind, size) ((Register){(kind), (size)})
#define reg64obj(kind) regobj(kind, OpSize64)
#define asmPushReg(pushreg)                                                              \
    do {                                                                                 \
        AsmInstOperand operand;                                                          \
        operand.mode = AsmAddressingModeRegister;                                        \
        operand.src.reg = (pushreg);                                                     \
        appendAsmInstPush(&asmlist, &operand);                                           \
    } while (0)
#define asmPushRax() asmPushReg(reg64obj(RAX))
#define asmPopRax() appendAsmInstPop(&asmlist, &reg64obj(RAX))
#define asmPrintPosition() appendAsmInstAnyText(&asmlist, "  # %s:%d", __FILE__, __LINE__)

static AsmInst *genCodeGVarInit(GVarInit *initializer) {
    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    for (GVarInit *elem = initializer; elem; elem = elem->next) {
        if (elem->kind == GVarInitZero) {
            appendAsmInstAnyText(&asmlist, "  .zero %d", elem->size);
        } else if (elem->kind == GVarInitNum) {
            switch (elem->size) {
            case 8:
                appendAsmInstAnyText(&asmlist, "  .quad %d", elem->rhs->val);
                break;
            case 4:
                appendAsmInstAnyText(&asmlist, "  .long %d", elem->rhs->val);
                break;
            case 1:
                appendAsmInstAnyText(&asmlist, "  .byte %d", elem->rhs->val);
                break;
            default:
                errorUnreachable();
            }
        } else if (elem->kind == GVarInitString) {
            appendAsmInstAnyText(
                    &asmlist, "  .string \"%s\"", elem->rhs->token->literalStr->string);
        } else if (elem->kind == GVarInitPointer) {
            if (elem->rhs->kind == NodeLiteralString) {
                appendAsmInstAnyText(&asmlist, "  .quad .LiteralString%d",
                        elem->rhs->token->literalStr->id);
            } else if (elem->rhs->kind == NodeGVar) {
                Token *token = initializer->rhs->token;
                appendAsmInstAnyText(&asmlist, "  .quad %.*s", token->len, token->str);
            } else if (elem->rhs->kind == NodeLVar) {
                appendAsmInstAnyText(
                        &asmlist, "  .quad .StaticVar%d", elem->rhs->obj->staticVarID);
            } else if (elem->rhs->kind == NodeNum) {
                appendAsmInstAnyText(&asmlist, "  .long %d", elem->rhs->val);
            } else {
                errorUnreachable();
            }
        } else {
            errorUnreachable();
        }
    }

    return getRawAsmInstList(&asmlist);
}

AsmInst *genAsmGlobals(void) {
    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (globals.globalVars == NULL && globals.staticVars == NULL &&
            globals.strings == NULL)
        return getRawAsmInstList(&asmlist);

    appendAsmInstAnyText(&asmlist, "\n.data");
    if (globals.literalStringCount) {
        LiteralString **strings = (LiteralString **)safeAlloc(
                globals.literalStringCount * sizeof(LiteralString *));

        // Reverse order.
        int index = globals.literalStringCount - 1;
        for (LiteralString *s = globals.strings; s; s = s->next) {
            strings[index--] = s;
        }

        for (int i = 0; i < globals.literalStringCount; ++i) {
            LiteralString *s = strings[i];
            appendAsmInstAnyText(&asmlist, ".LiteralString%d:", s->id);
            appendAsmInstAnyText(&asmlist, "  .string  \"%s\"", s->string);
        }

        safeFree(strings);
    }
    for (GVar *gvar = globals.globalVars; gvar; gvar = gvar->next) {
        Obj *v = gvar->obj;
        if (v->isExtern)
            continue;
        if (!v->isStatic) {
            appendAsmInstAnyText(&asmlist, ".globl %.*s", v->token->len, v->token->str);
        }
        appendAsmInstAnyText(&asmlist, "%.*s:", v->token->len, v->token->str);
        appendAsmInst(&asmlist, genCodeGVarInit(gvar->initializer));
    }
    for (GVar *v = globals.staticVars; v; v = v->next) {
        appendAsmInstAnyText(&asmlist, ".StaticVar%d:", v->obj->staticVarID);
        appendAsmInst(&asmlist, genCodeGVarInit(v->initializer));
    }

    return getRawAsmInstList(&asmlist);
}

static AsmInst *genCodeLVal(const Node *n) {
    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (!n)
        return getRawAsmInstList(&asmlist);

    if (n->kind == NodeDeref) {
        appendAsmInst(&asmlist, genAsm(n->rhs));
        // Address for variable must be on the top of the stack.
        return getRawAsmInstList(&asmlist);
    } else if (n->kind == NodeExprList) {
        Node *expr = n->body;
        if (!expr)
            errorAt(n->token, "Not a lvalue");
        while (expr->next)
            expr = expr->next;
        if (!isLvalue(expr))
            errorAt(expr->token, "Not a lvalue");
        appendAsmInst(&asmlist, genAsm(n));
        return getRawAsmInstList(&asmlist);
    } else if (!isLvalue(n)) {
        errorAt(n->token, "Not a lvalue");
    }

    // Make sure the value on the top of the stack is the memory address to lhs
    // variable.
    // When it's local variable, calculate it on rax from the rbp(base pointer)
    // and offset, then store it on the top of the stack.  Calculate it on rax,
    // not on rbp, because rbp must NOT be changed until exiting from a
    // function.
    if (n->kind == NodeGVar || (n->kind == NodeLVar && n->obj->isExtern)) {
        appendAsmInstAnyText(
                &asmlist, "  lea rax, %.*s[rip]", n->token->len, n->token->str);
        asmPushRax();
    } else if (n->kind == NodeMemberAccess) {
        StructOrUnion *objdef = n->lhs->type->type == TypeStruct ? n->lhs->type->structDef
                                                                 : n->lhs->type->unionDef;
        Obj *m = findStructOrUnionMember(objdef, n->token->str, n->token->len);
        appendAsmInst(&asmlist, genCodeLVal(n->lhs));
        asmPopRax();
        appendAsmInstAnyText(&asmlist, "  add rax, %d", m->offset);
        asmPushRax();
    } else if (n->kind == NodeLVar && n->obj->isStatic) {
        appendAsmInstAnyText(
                &asmlist, "  lea rax, .StaticVar%d[rip]", n->obj->staticVarID);
        asmPushRax();
    } else if (n->obj->type->type == TypeFunction) {
        appendAsmInstAnyText(&asmlist, "  mov rax, QWORD PTR %.*s@GOTPCREL[rip]",
                n->token->len, n->token->str);
        asmPushRax();
    } else {
        appendAsmInstAnyText(&asmlist, "  mov rax, rbp");
        appendAsmInstAnyText(&asmlist, "  sub rax, %d", n->obj->offset);
        asmPushRax();
    }

    return getRawAsmInstList(&asmlist);
}

// Generate code for dereferencing variables as rvalue.  If you need code for
// dereferencing variables as lvalue, use genCodeLVal() instead.
static AsmInst *genCodeDeref(const Node *n) {
    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (!n)
        return getRawAsmInstList(&asmlist);

    appendAsmInst(&asmlist, genCodeLVal(n));

    // Even when a value is treated as rvalue, we should left a memory address
    // on stack for values that cannot always assign to a register like struct,
    // array or function.
    if (!isRegisterStorableValue(n))
        return getRawAsmInstList(&asmlist);

    appendAsmInstAnyText(&asmlist, "  mov rax, [rsp]");
    switch (sizeOf(n->type)) {
    case 8:
        appendAsmInstAnyText(&asmlist, "  mov rax, [rax]");
        break;
    case 4:
        appendAsmInstAnyText(&asmlist, "  mov eax, DWORD PTR [rax]");
        appendAsmInstAnyText(&asmlist, "  movsx rax, eax");
        break;
    case 1:
        appendAsmInstAnyText(&asmlist, "  mov al, BYTE PTR [rax]");
        appendAsmInstAnyText(&asmlist, "  movsx rax, al");
        break;
    default:
        // appendAsmInstAnyText(&asmlist, "  lea rax, [rax]");
        // break;
        errorUnreachable();
    }
    appendAsmInstAnyText(&asmlist, "  mov [rsp], rax");

    return getRawAsmInstList(&asmlist);
}

static AsmInst *genCodeAssign(const Node *n) {
    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (!n)
        return getRawAsmInstList(&asmlist);

    appendAsmInst(&asmlist, genAsm(n->rhs));
    appendAsmInst(&asmlist, genCodeLVal(n->lhs));
    // Stack before assign:
    // |                     |
    // |       ......        |
    // +---------------------+
    // |     Rhs value       | --> Load to RDI.
    // +---------------------+
    // | Lhs memory address  | --> Load to RAX.
    // +---------------------+
    //
    // Stack after assign:
    // |                     |
    // |       ......        |
    // +---------------------+
    // |     Rhs value       | <-- Restore from RDI.
    // +---------------------+
    asmPopRax();
    appendAsmInstPop(&asmlist, &reg64obj(RDI));
    switch (sizeOf(n->type)) {
    case 8:
        appendAsmInstAnyText(&asmlist, "  mov [rax], rdi");
        break;
    case 4:
        appendAsmInstAnyText(&asmlist, "  mov DWORD PTR [rax], edi");
        break;
    case 1:
        appendAsmInstAnyText(&asmlist, "  mov BYTE PTR [rax], dil");
        break;
    default:
        errorUnreachable();
    }
    asmPushReg(reg64obj(RDI));

    return getRawAsmInstList(&asmlist);
}

static AsmInst *genCodeReturn(const Node *n) {
    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (!n)
        return getRawAsmInstList(&asmlist);

    if (n->lhs) {
        if (!isExprNode(n->lhs))
            errorAt(n->lhs->token, "Expression doesn't leave value.");
        appendAsmInst(&asmlist, genAsm(n->lhs));
        asmPopRax();
    }
    appendAsmInstAnyText(&asmlist, "  jmp .Lreturn_%.*s", dumpEnv.currentFunc->token->len,
            dumpEnv.currentFunc->token->str);

    return getRawAsmInstList(&asmlist);
}

static AsmInst *genCodeIf(const Node *n) {
    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (!n)
        return getRawAsmInstList(&asmlist);

    int elseblockCount = 0;
    appendAsmInst(&asmlist, genAsm(n->condition));
    asmPopRax();
    appendAsmInstAnyText(&asmlist, "  cmp rax, 0");
    if (n->elseblock) {
        appendAsmInstAnyText(&asmlist, "  je .Lelse%d_%d", n->blockID, elseblockCount);
    } else {
        appendAsmInstAnyText(&asmlist, "  je .Lend%d", n->blockID);
    }
    appendAsmInst(&asmlist, genAsm(n->body));
    if (isExprNode(n->body)) {
        asmPopRax();
    }

    if (n->elseblock) {
        appendAsmInstAnyText(&asmlist, "  jmp .Lend%d", n->blockID);
    }
    if (n->elseblock) {
        for (Node *e = n->elseblock; e; e = e->next) {
            appendAsmInstAnyText(&asmlist, ".Lelse%d_%d:", n->blockID, elseblockCount);
            ++elseblockCount;
            if (e->kind == NodeElseif) {
                appendAsmInst(&asmlist, genAsm(e->condition));
                asmPopRax();
                appendAsmInstAnyText(&asmlist, "  cmp rax, 0");
                if (e->next)
                    appendAsmInstAnyText(
                            &asmlist, "  je .Lelse%d_%d", n->blockID, elseblockCount);
                else // Last 'else' is omitted.
                    appendAsmInstAnyText(&asmlist, "  je .Lend%d", n->blockID);
            }
            appendAsmInst(&asmlist, genAsm(e->body));
            if (isExprNode(e->body)) {
                asmPopRax();
            }
            appendAsmInstAnyText(&asmlist, "  jmp .Lend%d", n->blockID);
        }
    }
    appendAsmInstAnyText(&asmlist, ".Lend%d:", n->blockID);

    return getRawAsmInstList(&asmlist);
}

static AsmInst *genCodeSwitch(const Node *n) {
    int haveDefaultLabel = 0;
    int loopBlockIDSave;

    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (!n)
        return getRawAsmInstList(&asmlist);

    loopBlockIDSave = dumpEnv.loopBlockID;
    dumpEnv.loopBlockID = n->blockID;
    appendAsmInst(&asmlist, genAsm(n->condition));
    asmPopRax();
    for (SwitchCase *c = n->cases; c; c = c->next) {
        if (!c->node->condition) {
            haveDefaultLabel = 1;
            continue;
        }
        appendAsmInstAnyText(&asmlist, "  mov rdi, %d", c->node->condition->val);
        appendAsmInstAnyText(&asmlist, "  cmp rax, rdi");
        appendAsmInstAnyText(&asmlist, "  je .Lswitch_case_%d_%d", n->blockID,
                c->node->condition->val);
    }
    if (haveDefaultLabel)
        appendAsmInstAnyText(&asmlist, "  jmp .Lswitch_default_%d", n->blockID);
    else
        appendAsmInstAnyText(&asmlist, "  jmp .Lend%d", n->blockID);

    appendAsmInst(&asmlist, genAsm(n->body));

    appendAsmInstAnyText(&asmlist, ".Lend%d:", n->blockID);

    dumpEnv.loopBlockID = loopBlockIDSave;

    return getRawAsmInstList(&asmlist);
}

static AsmInst *genCodeSwitchCase(const Node *n) {
    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (!n)
        return getRawAsmInstList(&asmlist);

    if (n->condition)
        appendAsmInstAnyText(
                &asmlist, ".Lswitch_case_%d_%d:", n->blockID, n->condition->val);
    else
        appendAsmInstAnyText(&asmlist, ".Lswitch_default_%d:", n->blockID);

    return getRawAsmInstList(&asmlist);
}

static AsmInst *genCodeFor(const Node *n) {
    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (!n)
        return getRawAsmInstList(&asmlist);

    int loopBlockIDSave = dumpEnv.loopBlockID;
    dumpEnv.loopBlockID = n->blockID;
    if (n->initializer) {
        appendAsmInst(&asmlist, genAsm(n->initializer));

        // Not always initializer statement left a value on stack.  E.g.
        // Variable declarations won't leave values on stack.
        if (isExprNode(n->initializer))
            asmPopRax();
    }
    appendAsmInstAnyText(&asmlist, ".Lbegin%d:", n->blockID);
    if (n->condition) {
        appendAsmInst(&asmlist, genAsm(n->condition));
    } else {
        appendAsmInstAnyText(&asmlist, "  push 1");
    }
    asmPopRax();
    appendAsmInstAnyText(&asmlist, "  cmp rax, 0");
    appendAsmInstAnyText(&asmlist, "  je .Lend%d", n->blockID);
    appendAsmInst(&asmlist, genAsm(n->body));
    if (n->body && isExprNode(n->body)) {
        asmPopRax();
    }
    appendAsmInstAnyText(&asmlist, ".Literator%d:", n->blockID);
    if (n->iterator) {
        appendAsmInst(&asmlist, genAsm(n->iterator));
        asmPopRax();
    }
    appendAsmInstAnyText(&asmlist, "  jmp .Lbegin%d", n->blockID);
    appendAsmInstAnyText(&asmlist, ".Lend%d:", n->blockID);
    dumpEnv.loopBlockID = loopBlockIDSave;

    return getRawAsmInstList(&asmlist);
}

static AsmInst *genCodeDoWhile(const Node *n) {
    int loopBlockIDSave = dumpEnv.loopBlockID;
    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (!n)
        return getRawAsmInstList(&asmlist);

    dumpEnv.loopBlockID = n->blockID;
    appendAsmInstAnyText(&asmlist, ".Lbegin%d:", n->blockID);

    appendAsmInst(&asmlist, genAsm(n->body));
    if (isExprNode(n->body)) {
        asmPopRax();
    }
    appendAsmInstAnyText(&asmlist, ".Literator%d:", n->blockID);
    appendAsmInst(&asmlist, genAsm(n->condition));
    asmPopRax();
    appendAsmInstAnyText(&asmlist, "  cmp rax, 0");
    appendAsmInstAnyText(&asmlist, "  jne .Lbegin%d", n->blockID);
    appendAsmInstAnyText(&asmlist, ".Lend%d:", n->blockID);

    dumpEnv.loopBlockID = loopBlockIDSave;

    return getRawAsmInstList(&asmlist);
}

static AsmInst *genCodeFCall(const Node *n) {
    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (!n)
        return getRawAsmInstList(&asmlist);

    static int stackAlignState = -1; // RSP % 16
    int stackAlignStateSave = 0;
    int stackVarSize = 0;    // Size of local variables on stack.
    int stackArgSize = 0;    // Size of arguments (passed to function) on stack.
    int exCapToAlignRSP = 0; // Extra memory size to capture in order to align RSP.
    int regargs = n->fcall->argsCount;
    int isSimpleFuncCall =
            n->body->type->type == TypeFunction; // TRUE if simple function call.

    if (!isSimpleFuncCall)
        appendAsmInst(&asmlist, genAsm(n->body));

    stackAlignStateSave = stackAlignState;

    if (regargs > REG_ARGS_MAX_COUNT)
        regargs = REG_ARGS_MAX_COUNT;

    if (stackAlignState == -1) {
        stackAlignState = 0;
        stackVarSize = dumpEnv.currentFunc->func->capStackSize;
    }

    if ((n->fcall->argsCount - regargs) > 0) {
        stackArgSize += (n->fcall->argsCount - regargs) * ONE_WORD_BYTES;
        stackVarSize += stackArgSize;
    }

    if (!isSimpleFuncCall)
        stackVarSize += ONE_WORD_BYTES; // Capacity for function pointer on stack

    stackAlignState = (stackAlignState + stackVarSize) % 16;
    if (stackAlignState)
        exCapToAlignRSP = 16 - stackAlignState;
    stackAlignState = 0;

    if (exCapToAlignRSP)
        // Align RSP to multiple of 16.
        appendAsmInstAnyText(
                &asmlist, "  sub rsp, %d /* RSP alignment */", exCapToAlignRSP);

    for (Node *c = n->fcall->args; c; c = c->next) {
        appendAsmInst(&asmlist, genAsm(c));
        if (isExprNode(c))
            stackAlignState += 8;
    }
    for (int i = 0; i < regargs; ++i)
        appendAsmInstPop(&asmlist,
                &regobj(argRegs[i], getOperandSizeFromByteSize(ONE_WORD_BYTES)));

    // Set AL to count of float arguments in variadic arguments area.  This is
    // always 0 now.
    appendAsmInstAnyText(&asmlist, "  mov al, 0");
    if (isSimpleFuncCall) {
        appendAsmInstAnyText(&asmlist, "  call %.*s", n->fcall->len, n->fcall->name);
    } else {
        appendAsmInstAnyText(
                &asmlist, "  mov r10, QWORD PTR %d[rsp]", exCapToAlignRSP + stackArgSize);
        appendAsmInstAnyText(&asmlist, "  call r10");
        appendAsmInstAnyText(&asmlist, "  add rsp, %d /* Throw away function pointer */",
                ONE_WORD_BYTES);
    }

    stackAlignState = stackAlignStateSave;
    if (exCapToAlignRSP)
        appendAsmInstAnyText(
                &asmlist, "  add rsp, %d /* RSP alignment */", exCapToAlignRSP);

    // Adjust RSP value when we used stack to pass arguments.
    if (stackArgSize)
        appendAsmInstAnyText(
                &asmlist, "  add rsp, %d /* Pop overflow args */", stackArgSize);

    asmPushRax();

    return getRawAsmInstList(&asmlist);
}

static AsmInst *genCodeVaStart(const Node *n) {
    Obj *lastArg = NULL;
    int offset = 0; // va_list member's offset currently watching
    int argsOverflows = 0;
    AsmInstList asmlist;

    initAsmInstList(&asmlist);

    for (Obj *arg = n->parentFunc->func->args; arg; arg = arg->next)
        if (!arg->next)
            lastArg = arg;

    appendAsmInst(&asmlist, genCodeLVal(n->fcall->args->next));
    asmPopRax();

    if (n->parentFunc->func->argsCount < REG_ARGS_MAX_COUNT) {
        appendAsmInstAnyText(&asmlist, "  mov DWORD PTR %d[rax], %d", offset,
                ONE_WORD_BYTES * (REG_ARGS_MAX_COUNT + 1) - lastArg->offset);
    } else {
        appendAsmInstAnyText(&asmlist, "  mov DWORD PTR %d[rax], %d", offset,
                ONE_WORD_BYTES * REG_ARGS_MAX_COUNT);
    }

    offset += sizeOf(&Types.Int);
    appendAsmInstAnyText(&asmlist, "  mov DWORD PTR %d[rax], %d", offset,
            REG_ARGS_MAX_COUNT * ONE_WORD_BYTES);

    offset += sizeOf(&Types.Int);
    if (n->parentFunc->func->argsCount <= REG_ARGS_MAX_COUNT) {
        appendAsmInstAnyText(&asmlist, "  lea rdi, %d[rbp]", ONE_WORD_BYTES * 2);
    } else {
        int overflow_reg_offset = -lastArg->offset + ONE_WORD_BYTES;
        appendAsmInstAnyText(&asmlist, "  lea rdi, %d[rbp]", overflow_reg_offset);
    }
    appendAsmInstAnyText(&asmlist, "  mov %d[rax], rdi", offset);

    offset += ONE_WORD_BYTES;
    appendAsmInstAnyText(
            &asmlist, "  lea rdi, %d[rbp]", -n->parentFunc->func->args->offset);
    appendAsmInstAnyText(&asmlist, "  mov %d[rax], rdi", offset);

    return getRawAsmInstList(&asmlist);
}

static AsmInst *genCodeFunction(const Node *n) {
    int regargs = 0;

    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (!n)
        return getRawAsmInstList(&asmlist);
    else if (dumpEnv.currentFunc)
        errorUnreachable();

    dumpEnv.currentFunc = n->obj;

    regargs = n->obj->func->argsCount;
    if (regargs > REG_ARGS_MAX_COUNT)
        regargs = REG_ARGS_MAX_COUNT;

    // asmDumpc('\n');
    appendAsmInstAnyText(&asmlist, ".section .text.startup,\"ax\",@progbits");
    if (!n->obj->isStatic) {
        appendAsmInstAnyText(
                &asmlist, ".globl %.*s", n->obj->token->len, n->obj->token->str);
    }
    appendAsmInstAnyText(&asmlist, "%.*s:", n->obj->token->len, n->obj->token->str);

    // Prologue.
    asmPushReg(reg64obj(RBP));
    appendAsmInstAnyText(&asmlist, "  mov rbp, rsp");
    if (n->obj->func->capStackSize)
        appendAsmInstAnyText(&asmlist, "  sub rsp, %d", n->obj->func->capStackSize);

    // Push arguments onto stacks from registers.
    if (regargs) {
        int count = 0;
        Obj *arg = n->obj->func->args;

        for (; count < regargs; ++count, arg = arg->next) {
            int size = sizeOf(arg->type);
            char *fmt;
            switch (sizeOf(arg->type)) {
            case 8:
                fmt = "  mov %d[rbp], %s";
                break;
            case 4:
                fmt = "  mov DWORD PTR %d[rbp], %s";
                break;
            case 1:
                fmt = "  mov BYTE PTR %d[rbp], %s";
                break;
            default:
                errorUnreachable();
            }
            appendAsmInstAnyText(
                    &asmlist, fmt, -arg->offset, getReg(argRegs[count], size));
        }
    }

    if (n->obj->func->haveVaArgs && regargs < REG_ARGS_MAX_COUNT) {
        int offset = 0;
        for (Obj *arg = n->obj->func->args; arg; arg = arg->next)
            if (!arg->next)
                offset = -arg->offset;
        for (int i = regargs; i < REG_ARGS_MAX_COUNT; ++i) {
            offset += ONE_WORD_BYTES;
            appendAsmInstAnyText(&asmlist, "  mov %d[rbp], %s", offset,
                    getReg(argRegs[i], ONE_WORD_BYTES));
        }
    }

    appendAsmInst(&asmlist, genAsm(n->body));

    // Epilogue
    if (n->obj->token->len == 4 && memcmp(n->obj->token->str, "main", 4) == 0)
        appendAsmInstAnyText(&asmlist, "  mov rax, 0");
    appendAsmInstAnyText(
            &asmlist, ".Lreturn_%.*s:", n->obj->token->len, n->obj->token->str);
    appendAsmInstAnyText(&asmlist, "  mov rsp, rbp");
    appendAsmInstAnyText(&asmlist, "  pop rbp");
    appendAsmInstAnyText(&asmlist, "  ret");
    appendAsmInstAnyText(&asmlist, ".section .note.GNU-stack,\"\",@progbits");

    dumpEnv.currentFunc = NULL;

    return getRawAsmInstList(&asmlist);
}

static AsmInst *genCodeIncrement(const Node *n, int prefix) {
    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (!n)
        return getRawAsmInstList(&asmlist);

    Node *expr = prefix ? n->rhs : n->lhs;
    appendAsmInst(&asmlist, genCodeLVal(expr));
    asmPopRax();
    appendAsmInstAnyText(&asmlist, "  mov rdi, rax");
    switch (sizeOf(n->type)) {
    case 8:
        appendAsmInstAnyText(&asmlist, "  mov rax, [rax]");
        break;
    case 4:
        appendAsmInstAnyText(&asmlist, "  mov eax, DWORD PTR [rax]");
        appendAsmInstAnyText(&asmlist, "  movsx rax, eax");
        break;
    case 1:
        appendAsmInstAnyText(&asmlist, "  mov al, BYTE PTR [rax]");
        appendAsmInstAnyText(&asmlist, "  movsx rax, al");
        break;
    default:
        errorUnreachable();
    }

    // Postfix increment operator refers the value before increment.
    if (!prefix)
        asmPushRax();

    switch (sizeOf(n->type)) {
    case 8:
        appendAsmInstAnyText(
                &asmlist, "  add rax, %d", getAlternativeOfOneForType(n->type));
        break;
    case 4:
        appendAsmInstAnyText(
                &asmlist, "  add eax, %d", getAlternativeOfOneForType(n->type));
        appendAsmInstAnyText(&asmlist, "  movsx rax, eax");
        break;
    case 1:
        appendAsmInstAnyText(
                &asmlist, "  add al, %d", getAlternativeOfOneForType(n->type));
        appendAsmInstAnyText(&asmlist, "  movsx rax, al");
        break;
    default:
        errorUnreachable();
    }

    // Prefix increment operator refers the value after increment.
    if (prefix)
        asmPushRax();

    // Reflect the expression result on variable.
    switch (sizeOf(n->type)) {
    case 8:
        appendAsmInstAnyText(&asmlist, "  mov [rdi], rax");
        break;
    case 4:
        appendAsmInstAnyText(&asmlist, "  mov DWORD PTR [rdi], eax");
        break;
    case 1:
        appendAsmInstAnyText(&asmlist, "  mov BYTE PTR [rdi], al");
        break;
    default:
        errorUnreachable();
    }

    return getRawAsmInstList(&asmlist);
}

static AsmInst *genCodeDecrement(const Node *n, int prefix) {
    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (!n)
        return getRawAsmInstList(&asmlist);

    Node *expr = prefix ? n->rhs : n->lhs;
    appendAsmInst(&asmlist, genCodeLVal(expr));
    asmPopRax();
    appendAsmInstAnyText(&asmlist, "  mov rdi, rax");
    switch (sizeOf(n->type)) {
    case 8:
        appendAsmInstAnyText(&asmlist, "  mov rax, [rax]");
        break;
    case 4:
        appendAsmInstAnyText(&asmlist, "  mov eax, DWORD PTR [rax]");
        appendAsmInstAnyText(&asmlist, "  movsx rax, eax");
        break;
    case 1:
        appendAsmInstAnyText(&asmlist, "  mov al, BYTE PTR [rax]");
        appendAsmInstAnyText(&asmlist, "  movsx rax, al");
        break;
    default:
        errorUnreachable();
    }

    // Postfix decrement operator refers the value before decrement.
    if (!prefix)
        asmPushRax();

    switch (sizeOf(n->type)) {
    case 8:
        appendAsmInstAnyText(
                &asmlist, "  sub rax, %d", getAlternativeOfOneForType(n->type));
        break;
    case 4:
        appendAsmInstAnyText(
                &asmlist, "  sub eax, %d", getAlternativeOfOneForType(n->type));
        appendAsmInstAnyText(&asmlist, "  movsx rax, eax");
        break;
    case 1:
        appendAsmInstAnyText(
                &asmlist, "  sub al, %d", getAlternativeOfOneForType(n->type));
        appendAsmInstAnyText(&asmlist, "  movsx rax, al");
        break;
    default:
        errorUnreachable();
    }

    // Prefix decrement operator refers the value after decrement.
    if (prefix)
        asmPushRax();

    // Reflect the expression result on variable.
    switch (sizeOf(n->type)) {
    case 8:
        appendAsmInstAnyText(&asmlist, "  mov [rdi], rax");
        break;
    case 4:
        appendAsmInstAnyText(&asmlist, "  mov DWORD PTR [rdi], eax");
        break;
    case 1:
        appendAsmInstAnyText(&asmlist, "  mov BYTE PTR [rdi], al");
        break;
    default:
        errorUnreachable();
    }

    return getRawAsmInstList(&asmlist);
}

static AsmInst *genCodeAdd(const Node *n) {
    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (!n)
        return getRawAsmInstList(&asmlist);

    int altOne = getAlternativeOfOneForType(n->type);
    appendAsmInst(&asmlist, genAsm(n->lhs));
    appendAsmInst(&asmlist, genAsm(n->rhs));

    if (isWorkLikePointer(n->lhs->type) || isWorkLikePointer(n->rhs->type)) {
        // Load integer to RAX and pointer to RDI in either case.
        if (isWorkLikePointer(n->lhs->type)) { // ptr + num
            asmPopRax();
            appendAsmInstPop(&asmlist, &reg64obj(RDI));
        } else { // num + ptr
            appendAsmInstPop(&asmlist, &reg64obj(RDI));
            asmPopRax();
        }
        appendAsmInstAnyText(&asmlist, "  mov rsi, %d", altOne);
        appendAsmInstAnyText(&asmlist, "  imul rax, rsi");
        appendAsmInstAnyText(&asmlist, "  add rax, rdi");
        asmPushRax();
    } else {
        appendAsmInstPop(&asmlist, &reg64obj(RDI));
        asmPopRax();
        switch (sizeOf(n->lhs->type)) {
        case 8:
            appendAsmInstAnyText(&asmlist, "  add rax, rdi");
            break;
        case 4:
            appendAsmInstAnyText(&asmlist, "  add eax, edi");
            appendAsmInstAnyText(&asmlist, "  movsx rax, eax");
            break;
        case 1:
            appendAsmInstAnyText(&asmlist, "  add al, dil");
            appendAsmInstAnyText(&asmlist, "  movsx rax, al");
            break;
        default:
            errorUnreachable();
        }
        asmPushRax();
    }

    return getRawAsmInstList(&asmlist);
}

static AsmInst *genCodeSub(const Node *n) {
    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (!n)
        return getRawAsmInstList(&asmlist);

    appendAsmInst(&asmlist, genAsm(n->lhs));
    appendAsmInst(&asmlist, genAsm(n->rhs));

    appendAsmInstPop(&asmlist, &reg64obj(RDI));
    asmPopRax();
    if (n->lhs->type->type == TypePointer) {
        int altOne = getAlternativeOfOneForType(n->lhs->type);
        int subBetweenPtr = n->type->type == TypePtrdiff_t;
        if (!subBetweenPtr) {
            appendAsmInstAnyText(&asmlist, "  mov rsi, %d", altOne);
            appendAsmInstAnyText(&asmlist, "  imul rdi, rsi");
        }
        appendAsmInstAnyText(&asmlist, "  sub rax, rdi");
        if (subBetweenPtr) {
            appendAsmInstAnyText(&asmlist, "  mov rsi, %d", altOne);
            appendAsmInstAnyText(&asmlist, "  cqo");
            appendAsmInstAnyText(&asmlist, "  idiv rsi");
        }
        asmPushRax();
    } else {
        // Subtraction between arithmetic types.
        // It should be that lhs, rhs, and result have all the same type.
        switch (sizeOf(n->type)) {
        case 8:
            appendAsmInstAnyText(&asmlist, "  sub rax, rdi");
            break;
        case 4:
            appendAsmInstAnyText(&asmlist, "  sub eax, edi");
            appendAsmInstAnyText(&asmlist, "  movsx rax, eax");
            break;
        case 1:
            appendAsmInstAnyText(&asmlist, "  sub al, dil");
            appendAsmInstAnyText(&asmlist, "  movsx rax, al");
            break;
        default:
            errorUnreachable();
        }
        asmPushRax();
    }

    return getRawAsmInstList(&asmlist);
}

static AsmInst *genCodeInitVarArray(const Node *n, TypeInfo *varType) {
    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (!n)
        return getRawAsmInstList(&asmlist);

    Node *var = n->lhs;

    if (varType->type == TypeArray) {
        if (n->rhs->kind == NodeInitList) {
            int index = 0;
            Node *initVal = n->rhs->body;
            for (; initVal; initVal = initVal->next, ++index) {
                Node constNum = {};
                Node elem = {};
                Node initNode = {};

                fillNodeNum(&constNum, index);
                fillNodeAdd(&elem, var, &constNum, varType);
                fillNodeInitVar(&initNode, var->token, &elem, initVal);

                appendAsmInst(
                        &asmlist, genCodeInitVarArray(&initNode, varType->baseType));
            }
        } else if (n->rhs->kind == NodeLiteralString) {
            LiteralString *string = n->rhs->token->literalStr;
            int len = 0;
            int elemIdx = 0;

            if (!string)
                errorUnreachable();

            len = strlen(string->string);

            // Checks for array size is done later.
            for (int i = 0; i < len; ++i) {
                Node chNode = {};
                Node numNode = {};
                Node shiftptr = {};
                Node elem = {};
                Node initNode = {};
                char c = string->string[i];

                if (c == '\\') {
                    char cc = 0;
                    if (checkEscapeChar(string->string[++i], &cc))
                        c = cc;
                    else
                        --i;
                }

                fillNodeNum(&chNode, (int)c);
                fillNodeNum(&numNode, elemIdx);
                fillNodeAdd(&shiftptr, var, &numNode, varType);

                elem.kind = NodeDeref;
                elem.rhs = &shiftptr;
                elem.type = varType->baseType;

                initNode.kind = NodeAssign;
                initNode.lhs = &elem;
                initNode.rhs = &chNode;
                initNode.type = elem.type;
                initNode.token = var->token;

                elemIdx++;

                appendAsmInst(&asmlist, genCodeAssign(&initNode));
                asmPopRax();
            }
        } else {
            errorUnreachable();
        }
    } else {
        Node deref = {};
        Node init = {};

        deref.kind = NodeDeref;
        deref.lhs = NULL;
        deref.rhs = var;
        deref.type = varType;
        deref.token = var->token;

        fillNodeInitVar(&init, var->token, &deref, n->rhs);

        appendAsmInst(&asmlist, genCodeInitVar(&init, deref.type));
    }

    return getRawAsmInstList(&asmlist);
}

static AsmInst *genCodeInitVarStruct(const Node *n, TypeInfo *varType) {
    if (varType->type != TypeStruct)
        errorUnreachable();

    Node *var = n->lhs;
    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (n->rhs->type->type == TypeStruct) {
        Node initNode = *n;
        initNode.kind = NodeAssignStruct;
        initNode.type = n->lhs->type;
        appendAsmInst(&asmlist, genAsm(&initNode));
        asmPopRax();
    } else {
        Node *initVal = n->rhs->body;
        Obj *member = var->type->structDef->members;

        for (; initVal && member; initVal = initVal->next, member = member->next) {
            Node access = {};
            Node initNode = {};

            access.kind = NodeMemberAccess;
            access.lhs = var;
            access.type = member->type;
            access.token = member->token;

            fillNodeInitVar(&initNode, var->token, &access, initVal);

            appendAsmInst(&asmlist, genAsm(&initNode));
        }
    }

    return getRawAsmInstList(&asmlist);
}

static AsmInst *genCodeInitVarUnion(const Node *n, TypeInfo *varType) {
    if (varType->type != TypeUnion)
        errorUnreachable();

    Node *var = n->lhs;
    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (n->rhs->type->type == TypeUnion) {
        Node initNode = *n;
        initNode.kind = NodeAssignUnion;
        initNode.type = n->lhs->type;
        appendAsmInst(&asmlist, genAsm(&initNode));
        asmPopRax();
    } else if (var->type->unionDef->members) {
        Node *initVal = n->rhs->body;
        Obj *initTarget = var->type->unionDef->members;
        Node access = {};
        Node initNode = {};

        access.kind = NodeMemberAccess;
        access.lhs = var;
        access.type = initTarget->type;
        access.token = initTarget->token;

        fillNodeInitVar(&initNode, var->token, &access, initVal);

        appendAsmInst(&asmlist, genAsm(&initNode));
    }

    return getRawAsmInstList(&asmlist);
}

static AsmInst *genCodeInitVar(const Node *n, TypeInfo *varType) {
    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (varType->type == TypeArray) {
        appendAsmInst(&asmlist, genCodeInitVarArray(n, varType));
    } else if (varType->type == TypeStruct) {
        appendAsmInst(&asmlist, genCodeInitVarStruct(n, varType));
    } else if (varType->type == TypeUnion) {
        appendAsmInst(&asmlist, genCodeInitVarUnion(n, varType));
    } else {
        // TODO: Support this: char *str = "...";
        Node copy = *n;
        copy.kind = NodeAssign;
        copy.type = varType;
        appendAsmInst(&asmlist, genCodeAssign(&copy));
        asmPopRax();
    }

    return getRawAsmInstList(&asmlist);
}

AsmInst *genAsm(const Node *n) {
    AsmInstList asmlist;
    initAsmInstList(&asmlist);

    if (!n)
        return getRawAsmInstList(&asmlist);

    if (n->kind == NodeNop) {
        // Do nothing.
    } else if (n->kind == NodeInitVar) {
        appendAsmInst(&asmlist, genCodeInitVar(n, n->lhs->type));
    } else if (n->kind == NodeClearStack) {
        int entire, rest;
        entire = rest = sizeOf(n->rhs->type);
        appendAsmInstAnyText(&asmlist, "  mov rax, rbp");
        appendAsmInstAnyText(&asmlist, "  sub rax, %d", n->rhs->obj->offset);
        while (rest) {
            if (rest >= 8) {
                appendAsmInstAnyText(
                        &asmlist, "  mov QWORD PTR %d[rax], 0", entire - rest);
                rest -= 8;
            } else if (rest >= 4) {
                appendAsmInstAnyText(
                        &asmlist, "  mov DWORD PTR %d[rax], 0", entire - rest);
                rest -= 4;
            } else {
                appendAsmInstAnyText(
                        &asmlist, "  mov BYTE PTR %d[rax], 0", entire - rest);
                rest -= 1;
            }
        }
    } else if (n->kind == NodeTypeCast) {
        int destSize;
        destSize = sizeOf(n->type);

        appendAsmInst(&asmlist, genAsm(n->rhs));

        // Currently, cast is needed only when destSize < 8.
        if (destSize >= 8)
            return getRawAsmInstList(&asmlist);

        asmPopRax();
        switch (destSize) {
        case 4:
            appendAsmInstAnyText(
                    &asmlist, "  movsx rax, eax"); // We only have signed variables yet.
            break;
        case 1:
            appendAsmInstAnyText(
                    &asmlist, "  movsx rax, al"); // We only have signed variable yet.
            break;
        default:
            errorUnreachable();
        }
        asmPushRax();
    } else if (n->kind == NodeAddress) {
        appendAsmInst(&asmlist, genCodeLVal(n->rhs));
    } else if (n->kind == NodeDeref) {
        appendAsmInst(&asmlist, genCodeDeref(n));
    } else if (n->kind == NodeBlock) {
        for (Node *c = n->body; c; c = c->next) {
            appendAsmInst(&asmlist, genAsm(c));
            // Statement lefts a value on the top of the stack, and it should
            // be thrown away. (But Block node does not put any value, so do
            // not pop value.)
            if (isExprNode(c)) {
                asmPopRax();
            }
        }
    } else if (n->kind == NodeExprList) {
        if (!n->body) {
            appendAsmInstAnyText(&asmlist, "  push 0  /* Represents NOP */");
            return getRawAsmInstList(&asmlist);
        }
        for (Node *c = n->body; c; c = c->next) {
            appendAsmInst(&asmlist, genAsm(c));
            // Throw away values that expressions left on stack, but the last
            // expression is the exception and its result value may be used in
            // next statement.
            if (isExprNode(c) && c->next)
                asmPopRax();
        }
    } else if (n->kind == NodeInitList) {
        errorUnreachable();
    } else if (n->kind == NodeNot) {
        // TODO: Make sure n->rhs lefts a value on stack
        appendAsmInst(&asmlist, genAsm(n->rhs));
        asmPopRax();
        appendAsmInstAnyText(&asmlist, "  cmp rax, 0");
        appendAsmInstAnyText(&asmlist, "  sete al");
        appendAsmInstAnyText(&asmlist, "  movzb rax, al");
        asmPushRax();
    } else if (n->kind == NodeLogicalAND) {
        // TODO: Make sure n->rhs lefts a value on stack
        appendAsmInst(&asmlist, genAsm(n->lhs));
        asmPopRax();
        appendAsmInstAnyText(&asmlist, "  cmp rax, 0");
        appendAsmInstAnyText(&asmlist, "  je .Llogicaland%d", n->blockID);
        appendAsmInst(&asmlist, genAsm(n->rhs));
        asmPopRax();
        appendAsmInstAnyText(&asmlist, "  cmp rax, 0");
        appendAsmInstAnyText(&asmlist, ".Llogicaland%d:", n->blockID);
        appendAsmInstAnyText(&asmlist, "  setne al");
        appendAsmInstAnyText(&asmlist, "  movzb rax, al");
        asmPushRax();
    } else if (n->kind == NodeLogicalOR) {
        appendAsmInst(&asmlist, genAsm(n->lhs));
        asmPopRax();
        appendAsmInstAnyText(&asmlist, "  cmp rax, 0");
        appendAsmInstAnyText(&asmlist, "  jne .Llogicalor%d", n->blockID);
        appendAsmInst(&asmlist, genAsm(n->rhs));
        asmPopRax();
        appendAsmInstAnyText(&asmlist, "  cmp rax, 0");
        appendAsmInstAnyText(&asmlist, ".Llogicalor%d:", n->blockID);
        appendAsmInstAnyText(&asmlist, "  setne al");
        appendAsmInstAnyText(&asmlist, "  movzb rax, al");
        asmPushRax();
    } else if (n->kind == NodeArithShiftL || n->kind == NodeArithShiftR) {
        char *op = n->kind == NodeArithShiftL ? "sal" : "sar";
        appendAsmInst(&asmlist, genAsm(n->lhs));
        appendAsmInst(&asmlist, genAsm(n->rhs));
        appendAsmInstAnyText(&asmlist, "  pop rcx");
        asmPopRax();
        appendAsmInstAnyText(&asmlist, "  %s eax, cl", op);
        appendAsmInstAnyText(&asmlist, "  movsx rax, eax");
        asmPushRax();
    } else if (n->kind == NodeNum) {
        appendAsmInstAnyText(&asmlist, "  push %d", n->val);
    } else if (n->kind == NodeLiteralString) {
        appendAsmInstAnyText(
                &asmlist, "  lea rax, .LiteralString%d[rip]", n->token->literalStr->id);
        asmPushRax();
    } else if (n->kind == NodeLVar || n->kind == NodeGVar ||
               n->kind == NodeMemberAccess) {
        // When NodeLVar appears with itself alone, it should be treated as a
        // rvalue, not a lvalue.
        appendAsmInst(&asmlist, genCodeLVal(n));

        // But, array, struct, and function are exceptions.  They work like
        // pointers even when they're being rvalues since they cannot always be
        // stored on register.
        if (!isRegisterStorableValue(n))
            return getRawAsmInstList(&asmlist);

        // In order to change this lvalue into rvalue, push a value of a
        // variable to the top of the stack.
        asmPopRax();
        switch (sizeOf(n->type)) {
        case 8:
            appendAsmInstAnyText(&asmlist, "  mov rax, [rax]");
            break;
        case 4:
            appendAsmInstAnyText(&asmlist, "  mov eax, DWORD PTR [rax]");
            appendAsmInstAnyText(&asmlist, "  movsx rax, eax");
            break;
        case 1:
            appendAsmInstAnyText(&asmlist, "  mov al, BYTE PTR [rax]");
            appendAsmInstAnyText(&asmlist, "  movsx rax, al");
            break;
        default:
            errorUnreachable();
        }
        asmPushRax();

    } else if (n->kind == NodeAssign) {
        appendAsmInst(&asmlist, genCodeAssign(n));
    } else if (n->kind == NodeAssignStruct || n->kind == NodeAssignUnion) {
        int total, rest;
        total = rest = sizeOf(n->type);
        appendAsmInst(&asmlist, genCodeLVal(n->lhs));
        appendAsmInst(&asmlist, genAsm(n->rhs));
        appendAsmInstPop(&asmlist, &reg64obj(RDI));
        asmPopRax();
        while (rest) {
            if (rest >= 8) {
                appendAsmInstAnyText(
                        &asmlist, "  mov rsi, QWORD PTR %d[rdi]", total - rest);
                appendAsmInstAnyText(
                        &asmlist, "  mov QWORD PTR %d[rax], rsi", total - rest);
                rest -= 8;
            } else if (rest >= 4) {
                appendAsmInstAnyText(
                        &asmlist, "  mov esi, DWORD PTR %d[rdi]", total - rest);
                appendAsmInstAnyText(
                        &asmlist, "  mov DWORD PTR %d[rax], esi", total - rest);
                rest -= 4;
            } else {
                appendAsmInstAnyText(
                        &asmlist, "  mov sil, BYTE PTR %d[rdi]", total - rest);
                appendAsmInstAnyText(
                        &asmlist, "  mov BYTE PTR %d[rax], sil", total - rest);
                rest -= 1;
            }
        }
        asmPushRax();
    } else if (n->kind == NodeBreak) {
        appendAsmInstAnyText(&asmlist, "  jmp .Lend%d", dumpEnv.loopBlockID);
    } else if (n->kind == NodeContinue) {
        appendAsmInstAnyText(&asmlist, "  jmp .Literator%d", dumpEnv.loopBlockID);
    } else if (n->kind == NodeReturn) {
        appendAsmInst(&asmlist, genCodeReturn(n));
    } else if (n->kind == NodeConditional) {
        appendAsmInst(&asmlist, genAsm(n->condition));
        asmPopRax();
        appendAsmInstAnyText(&asmlist, "  cmp rax, 0");
        appendAsmInstAnyText(&asmlist, "  je .Lcond_falsy_%d", n->blockID);
        appendAsmInst(&asmlist, genAsm(n->lhs));
        appendAsmInstAnyText(&asmlist, "  jmp .Lcond_end_%d", n->blockID);
        appendAsmInstAnyText(&asmlist, ".Lcond_falsy_%d:", n->blockID);
        appendAsmInst(&asmlist, genAsm(n->rhs));
        appendAsmInstAnyText(&asmlist, ".Lcond_end_%d:", n->blockID);
    } else if (n->kind == NodeIf) {
        appendAsmInst(&asmlist, genCodeIf(n));
    } else if (n->kind == NodeSwitch) {
        appendAsmInst(&asmlist, genCodeSwitch(n));
    } else if (n->kind == NodeSwitchCase) {
        appendAsmInst(&asmlist, genCodeSwitchCase(n));
    } else if (n->kind == NodeFor) {
        appendAsmInst(&asmlist, genCodeFor(n));
    } else if (n->kind == NodeDoWhile) {
        appendAsmInst(&asmlist, genCodeDoWhile(n));
    } else if (n->kind == NodeFCall) {
        appendAsmInst(&asmlist, genCodeFCall(n));
    } else if (n->kind == NodeVaStart) {
        appendAsmInst(&asmlist, genCodeVaStart(n));
    } else if (n->kind == NodeFunction) {
        appendAsmInst(&asmlist, genCodeFunction(n));
    } else if (n->kind == NodePreIncl || n->kind == NodePostIncl) {
        appendAsmInst(&asmlist, genCodeIncrement(n, n->kind == NodePreIncl));
    } else if (n->kind == NodePreDecl || n->kind == NodePostDecl) {
        appendAsmInst(&asmlist, genCodeDecrement(n, n->kind == NodePreDecl));
    } else if (n->kind == NodeAdd) {
        appendAsmInst(&asmlist, genCodeAdd(n));
    } else if (n->kind == NodeSub) {
        appendAsmInst(&asmlist, genCodeSub(n));
    } else {
        appendAsmInst(&asmlist, genAsm(n->lhs));
        appendAsmInst(&asmlist, genAsm(n->rhs));

        appendAsmInstPop(&asmlist, &reg64obj(RDI));
        asmPopRax();

        // Maybe these oprands should use only 8bytes registers.
        if (n->kind == NodeMul) {
            appendAsmInstAnyText(&asmlist, "  imul rax, rdi");
        } else if (n->kind == NodeDiv) {
            appendAsmInstAnyText(&asmlist, "  cqo");
            appendAsmInstAnyText(&asmlist, "  idiv rdi");
        } else if (n->kind == NodeDivRem) {
            appendAsmInstAnyText(&asmlist, "  cqo");
            appendAsmInstAnyText(&asmlist, "  idiv rdi");
            asmPushReg(reg64obj(RDX));
            return getRawAsmInstList(&asmlist);
        } else if (n->kind == NodeEq) {
            appendAsmInstAnyText(&asmlist, "  cmp rax, rdi");
            appendAsmInstAnyText(&asmlist, "  sete al");
            appendAsmInstAnyText(&asmlist, "  movzb rax, al");
        } else if (n->kind == NodeNeq) {
            appendAsmInstAnyText(&asmlist, "  cmp rax, rdi");
            appendAsmInstAnyText(&asmlist, "  setne al");
            appendAsmInstAnyText(&asmlist, "  movzb rax, al");
        } else if (n->kind == NodeLT) {
            appendAsmInstAnyText(&asmlist, "  cmp rax, rdi");
            appendAsmInstAnyText(&asmlist, "  setl al");
            appendAsmInstAnyText(&asmlist, "  movzb rax, al");
        } else if (n->kind == NodeLE) {
            appendAsmInstAnyText(&asmlist, "  cmp rax, rdi");
            appendAsmInstAnyText(&asmlist, "  setle al");
            appendAsmInstAnyText(&asmlist, "  movzb rax, al");
        } else if (n->kind == NodeBitwiseAND) {
            appendAsmInstAnyText(&asmlist, "  and rax, rdi");
        } else if (n->kind == NodeBitwiseOR) {
            appendAsmInstAnyText(&asmlist, "  or rax, rdi");
        } else if (n->kind == NodeBitwiseXOR) {
            appendAsmInstAnyText(&asmlist, "  xor rax, rdi");
        } else {
            errorUnreachable();
        }

        asmPushRax();
    }

    return getRawAsmInstList(&asmlist);
}

/**
 * Optimize the given assembly instructions.
 * Returns TRUE if the given insturctions are modified.
 */
void optimizeAsm(AsmInst *inst) {
    int modified = 0;
    AsmInst *top = inst;
    for (; inst; inst = inst->next) {
        switch (inst->kind) {
        case AsmMov:
            if (isEqualRegisterOperand(&inst->data.mov.src, &inst->data.mov.dst)) {
                AsmInst *next = inst->next;

                *inst = *inst->next;
                freeAsmInst(next);

                modified = 1;
            }
            break;
        case AsmPush:
            if (inst->next && inst->next->kind == AsmPop) {
                AsmInstOperand *src = &inst->data.push;
                AsmInst *next = inst->next;

                inst->kind = AsmMov;
                inst->data.mov.src = *src;
                inst->data.mov.dst.mode = AsmAddressingModeRegister;
                inst->data.mov.dst.src.reg = next->data.pop;

                inst->next = next->next;

                freeAsmInst(next);

                modified = 1;
            }
            break;
        default:
            break;
        }
    }

    if (modified) {
        optimizeAsm(top);
    }
}
