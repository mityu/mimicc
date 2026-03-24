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
static DumpEnv dumpEnv;
static void genCodeInitVarArray(const Node *n, TypeInfo *varType);
static void genCodeInitVarStruct(const Node *n, TypeInfo *varType);
static void genCodeInitVar(const Node *n, TypeInfo *varType);

static AsmInst asmCodeHead = {};
static AsmInst *curAsm = &asmCodeHead;
const AsmInst *getAsm(void) {
    return asmCodeHead.next;
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

static AsmInst *newAsmInstAnyText(char *text) {
    AsmInst *inst = newAsmInst(AsmAnyText);
    inst->text = text;
    return inst;
}

static void concatAsmInst(AsmInst *former, AsmInst *latter) {
    if (former->next) {
        error("`former` already has next element.");
    }
    former->next = latter;
}

/**
 * Get the last AsmInst element of a given list of AsmInst.
 */
static AsmInst *getLastAsmInst(AsmInst *inst) {
    for (; inst->next; inst = inst->next)
        ;
    return inst;
}

/**
 * Like sprintf(), but with safe and automatic allocation of a new memory.
 * Returns the pointer to newly allocated memory with contents of formatted string.
 */
static char *format(const char *fmt, ...) {
    int n = 0;
    char *stack;
    va_list ap;

    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    if (n < 0) {
        va_end(ap);
        error("vsnprintf() error: returned: %d", n);
    }
    va_end(ap);

    va_start(ap, fmt);
    stack = safeAlloc(sizeof(char) * (++n)); // One more space for NUL at the end of string.
    vsnprintf(stack, n, fmt, ap);
    va_end(ap);
    return stack;
}

static int asmDumps(char *s) {
    curAsm->next = newAsmInstAnyText(s);
    curAsm = curAsm->next;
}

static int asmDumpf(const char *fmt, ...) {
    int n = 0;
    char *stack;
    va_list ap;

    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    if (n < 0) {
        va_end(ap);
        error("vsnprintf() error: returned: %d", n);
    }
    va_end(ap);

    va_start(ap, fmt);
    stack = safeAlloc(sizeof(char) * (++n)); // One more space for NUL at the end of string.
    vsnprintf(stack, n, fmt, ap);
    va_end(ap);

    curAsm->next = newAsmInstAnyText(stack);
    curAsm = curAsm->next;
}

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

// const int Reg8 = 3;
// const int Reg16 = 2;
// const int Reg32 = 1;
// const int Reg64 = 0;

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

static void genCodeGVarInit(GVarInit *initializer) {
    for (GVarInit *elem = initializer; elem; elem = elem->next) {
        if (elem->kind == GVarInitZero) {
            asmDumpf("  .zero %d\n", elem->size);
        } else if (elem->kind == GVarInitNum) {
            switch (elem->size) {
            case 8:
                asmDumpf("  .quad %d\n", elem->rhs->val);
                break;
            case 4:
                asmDumpf("  .long %d\n", elem->rhs->val);
                break;
            case 1:
                asmDumpf("  .byte %d\n", elem->rhs->val);
                break;
            default:
                errorUnreachable();
            }
        } else if (elem->kind == GVarInitString) {
            asmDumpf("  .string \"%s\"\n", elem->rhs->token->literalStr->string);
        } else if (elem->kind == GVarInitPointer) {
            if (elem->rhs->kind == NodeLiteralString) {
                asmDumpf("  .quad .LiteralString%d\n", elem->rhs->token->literalStr->id);
            } else if (elem->rhs->kind == NodeGVar) {
                Token *token = initializer->rhs->token;
                asmDumpf("  .quad %.*s\n", token->len, token->str);
            } else if (elem->rhs->kind == NodeLVar) {
                asmDumpf("  .quad .StaticVar%d\n", elem->rhs->obj->staticVarID);
            } else if (elem->rhs->kind == NodeNum) {
                asmDumpf("  .long %d\n", elem->rhs->val);
            } else {
                errorUnreachable();
            }
        } else {
            errorUnreachable();
        }
    }
}

void genAsmGlobals(void) {
    if (globals.globalVars == NULL && globals.staticVars == NULL &&
            globals.strings == NULL)
        return;
    asmDumps("\n.data");
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
            asmDumpf(".LiteralString%d:\n", s->id);
            asmDumpf("  .string  \"%s\"\n", s->string);
        }

        safeFree(strings);
    }
    for (GVar *gvar = globals.globalVars; gvar; gvar = gvar->next) {
        Obj *v = gvar->obj;
        if (v->isExtern)
            continue;
        if (!v->isStatic) {
            asmDumpf(".globl %.*s\n", v->token->len, v->token->str);
        }
        asmDumpf("%.*s:\n", v->token->len, v->token->str);
        genCodeGVarInit(gvar->initializer);
    }
    for (GVar *v = globals.staticVars; v; v = v->next) {
        asmDumpf(".StaticVar%d:\n", v->obj->staticVarID);
        genCodeGVarInit(v->initializer);
    }
}

static void genCodeLVal(const Node *n) {
    if (!n)
        return;

    if (n->kind == NodeDeref) {
        genAsm(n->rhs);
        // Address for variable must be on the top of the stack.
        return;
    } else if (n->kind == NodeExprList) {
        Node *expr = n->body;
        if (!expr)
            errorAt(n->token, "Not a lvalue");
        while (expr->next)
            expr = expr->next;
        if (!isLvalue(expr))
            errorAt(expr->token, "Not a lvalue");
        genAsm(n);
        return;
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
        asmDumpf("  lea rax, %.*s[rip]\n", n->token->len, n->token->str);
        asmDumps("  push rax");
    } else if (n->kind == NodeMemberAccess) {
        Obj *m = findStructMember(n->lhs->type->structDef, n->token->str, n->token->len);
        genCodeLVal(n->lhs);
        asmDumps("  pop rax");
        asmDumpf("  add rax, %d\n", m->offset);
        asmDumps("  push rax");
    } else if (n->kind == NodeLVar && n->obj->isStatic) {
        asmDumpf("  lea rax, .StaticVar%d[rip]\n", n->obj->staticVarID);
        asmDumps("  push rax");
    } else if (n->obj->type->type == TypeFunction) {
        asmDumpf("  mov rax, QWORD PTR %.*s@GOTPCREL[rip]\n", n->token->len,
                n->token->str);
        asmDumps("  push rax");
    } else {
        asmDumps("  mov rax, rbp");
        asmDumpf("  sub rax, %d\n", n->obj->offset);
        asmDumps("  push rax");
    }
}

// Generate code dereferencing variables as rvalue.  If code for dereference as
// lvalue, use genCodeLVal() instead.
static void genCodeDeref(const Node *n) {
    if (!n)
        return;

    genCodeLVal(n);
    asmDumps("  mov rax, [rsp]");
    switch (sizeOf(n->type)) {
    case 8:
        asmDumps("  mov rax, [rax]");
        break;
    case 4:
        asmDumps("  mov eax, DWORD PTR [rax]");
        asmDumps("  movsx rax, eax");
        break;
    case 1:
        asmDumps("  mov al, BYTE PTR [rax]");
        asmDumps("  movsx rax, al");
        break;
    default:
        asmDumps("  lea rax, [rax]");
        break;
    }
    asmDumps("  mov [rsp], rax");
}

static void genCodeAssign(const Node *n) {
    if (!n)
        return;

    genAsm(n->rhs);
    genCodeLVal(n->lhs);

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
    asmDumps("  pop rax");
    asmDumps("  pop rdi");
    switch (sizeOf(n->type)) {
    case 8:
        asmDumps("  mov [rax], rdi");
        break;
    case 4:
        asmDumps("  mov DWORD PTR [rax], edi");
        break;
    case 1:
        asmDumps("  mov BYTE PTR [rax], dil");
        break;
    default:
        errorUnreachable();
    }
    asmDumps("  push rdi");
}

static void genCodeReturn(const Node *n) {
    if (!n)
        return;

    if (n->lhs) {
        if (!isExprNode(n->lhs))
            errorAt(n->lhs->token, "Expression doesn't left value.");
        genAsm(n->lhs);
        asmDumps("  pop rax");
    }
    asmDumpf("  jmp .Lreturn_%.*s\n", dumpEnv.currentFunc->token->len,
            dumpEnv.currentFunc->token->str);
}

static void genCodeIf(const Node *n) {
    if (!n)
        return;

    int elseblockCount = 0;
    genAsm(n->condition);
    asmDumps("  pop rax");
    asmDumps("  cmp rax, 0");
    if (n->elseblock) {
        asmDumpf("  je .Lelse%d_%d\n", n->blockID, elseblockCount);
    } else {
        asmDumpf("  je .Lend%d\n", n->blockID);
    }
    genAsm(n->body);
    if (isExprNode(n->body)) {
        asmDumps("  pop rax");
    }

    if (n->elseblock) {
        asmDumpf("  jmp .Lend%d\n", n->blockID);
    }
    if (n->elseblock) {
        for (Node *e = n->elseblock; e; e = e->next) {
            asmDumpf(".Lelse%d_%d:\n", n->blockID, elseblockCount);
            ++elseblockCount;
            if (e->kind == NodeElseif) {
                genAsm(e->condition);
                asmDumps("  pop rax");
                asmDumps("  cmp rax, 0");
                if (e->next)
                    asmDumpf("  je .Lelse%d_%d\n", n->blockID, elseblockCount);
                else // Last 'else' is omitted.
                    asmDumpf("  je .Lend%d\n", n->blockID);
            }
            genAsm(e->body);
            if (isExprNode(e->body)) {
                asmDumps("  pop rax");
            }
            asmDumpf("  jmp .Lend%d\n", n->blockID);
        }
    }
    asmDumpf(".Lend%d:\n", n->blockID);
}

static void genCodeSwitch(const Node *n) {
    int haveDefaultLabel = 0;
    int loopBlockIDSave;

    if (!n)
        return;

    loopBlockIDSave = dumpEnv.loopBlockID;
    dumpEnv.loopBlockID = n->blockID;
    genAsm(n->condition);
    asmDumps("  pop rax");
    for (SwitchCase *c = n->cases; c; c = c->next) {
        if (!c->node->condition) {
            haveDefaultLabel = 1;
            continue;
        }
        asmDumpf("  mov rdi, %d\n", c->node->condition->val);
        asmDumps("  cmp rax, rdi");
        asmDumpf("  je .Lswitch_case_%d_%d\n", n->blockID, c->node->condition->val);
    }
    if (haveDefaultLabel)
        asmDumpf("  jmp .Lswitch_default_%d\n", n->blockID);
    else
        asmDumpf("  jmp .Lend%d\n", n->blockID);

    genAsm(n->body);

    asmDumpf(".Lend%d:\n", n->blockID);

    dumpEnv.loopBlockID = loopBlockIDSave;
}

static void genCodeSwitchCase(const Node *n) {
    if (!n)
        return;

    if (n->condition)
        asmDumpf(".Lswitch_case_%d_%d:\n", n->blockID, n->condition->val);
    else
        asmDumpf(".Lswitch_default_%d:\n", n->blockID);
}

static void genCodeFor(const Node *n) {
    if (!n)
        return;

    int loopBlockIDSave = dumpEnv.loopBlockID;
    dumpEnv.loopBlockID = n->blockID;
    if (n->initializer) {
        genAsm(n->initializer);

        // Not always initializer statement left a value on stack.  E.g.
        // Variable declarations won't left values on stack.
        if (isExprNode(n->initializer))
            asmDumps("  pop rax");
    }
    asmDumpf(".Lbegin%d:\n", n->blockID);
    if (n->condition) {
        genAsm(n->condition);
    } else {
        asmDumps("  push 1");
    }
    asmDumps("  pop rax");
    asmDumps("  cmp rax, 0");
    asmDumpf("  je .Lend%d\n", n->blockID);
    genAsm(n->body);
    if (n->body && isExprNode(n->body)) {
        asmDumps("  pop rax");
    }
    asmDumpf(".Literator%d:\n", n->blockID);
    if (n->iterator) {
        genAsm(n->iterator);
        asmDumps("  pop rax");
    }
    asmDumpf("  jmp .Lbegin%d\n", n->blockID);
    asmDumpf(".Lend%d:\n", n->blockID);
    dumpEnv.loopBlockID = loopBlockIDSave;
}

static void genCodeDoWhile(const Node *n) {
    int loopBlockIDSave = dumpEnv.loopBlockID;
    if (!n)
        return;

    dumpEnv.loopBlockID = n->blockID;
    asmDumpf(".Lbegin%d:\n", n->blockID);

    genAsm(n->body);
    if (isExprNode(n->body)) {
        asmDumps("  pop rax");
    }
    asmDumpf(".Literator%d:\n", n->blockID);
    genAsm(n->condition);
    asmDumps("  pop rax");
    asmDumps("  cmp rax, 0");
    asmDumpf("  jne .Lbegin%d\n", n->blockID);
    asmDumpf(".Lend%d:\n", n->blockID);

    dumpEnv.loopBlockID = loopBlockIDSave;
}

static void genCodeFCall(const Node *n) {
    if (!n)
        return;

    static int stackAlignState = -1; // RSP % 16
    int stackAlignStateSave = 0;
    int stackVarSize = 0;    // Size of local variables on stack.
    int stackArgSize = 0;    // Size of arguments (passed to function) on stack.
    int exCapToAlignRSP = 0; // Extra memory size to capture in order to align RSP.
    int regargs = n->fcall->argsCount;
    int isSimpleFuncCall =
            n->body->type->type == TypeFunction; // TRUE if simple function call.

    if (!isSimpleFuncCall)
        genAsm(n->body);

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
        asmDumpf("  sub rsp, %d /* RSP alignment */\n", exCapToAlignRSP);

    for (Node *c = n->fcall->args; c; c = c->next) {
        genAsm(c);
        if (isExprNode(c))
            stackAlignState += 8;
    }
    for (int i = 0; i < regargs; ++i)
        asmDumpf("  pop %s\n", getReg(argRegs[i], ONE_WORD_BYTES));

    // Set AL to count of float arguments in variadic arguments area.  This is
    // always 0 now.
    asmDumps("  mov al, 0");
    if (isSimpleFuncCall) {
        asmDumpf("  call %.*s\n", n->fcall->len, n->fcall->name);
    } else {
        asmDumpf("  mov r10, QWORD PTR %d[rsp]\n", exCapToAlignRSP + stackArgSize);
        asmDumps("  call r10");
        asmDumpf("  add rsp, %d /* Throw away function pointer */\n", ONE_WORD_BYTES);
    }

    stackAlignState = stackAlignStateSave;
    if (exCapToAlignRSP)
        asmDumpf("  add rsp, %d /* RSP alignment */\n", exCapToAlignRSP);

    // Adjust RSP value when we used stack to pass arguments.
    if (stackArgSize)
        asmDumpf("  add rsp, %d /* Pop overflow args */\n", stackArgSize);

    asmDumps("  push rax");
}

static void genCodeVaStart(const Node *n) {
    Obj *lastArg = NULL;
    int offset = 0; // va_list member's offset currently watching
    int argsOverflows = 0;

    for (Obj *arg = n->parentFunc->func->args; arg; arg = arg->next)
        if (!arg->next)
            lastArg = arg;

    genCodeLVal(n->fcall->args->next);
    asmDumps("  pop rax");

    if (n->parentFunc->func->argsCount < REG_ARGS_MAX_COUNT) {
        asmDumpf("  mov DWORD PTR %d[rax], %d\n", offset,
                ONE_WORD_BYTES * (REG_ARGS_MAX_COUNT + 1) - lastArg->offset);
    } else {
        asmDumpf("  mov DWORD PTR %d[rax], %d\n", offset,
                ONE_WORD_BYTES * REG_ARGS_MAX_COUNT);
    }

    offset += sizeOf(&Types.Int);
    asmDumpf(
            "  mov DWORD PTR %d[rax], %d\n", offset, REG_ARGS_MAX_COUNT * ONE_WORD_BYTES);

    offset += sizeOf(&Types.Int);
    if (n->parentFunc->func->argsCount <= REG_ARGS_MAX_COUNT) {
        asmDumpf("  lea rdi, %d[rbp]\n", ONE_WORD_BYTES * 2);
    } else {
        int overflow_reg_offset = -lastArg->offset + ONE_WORD_BYTES;
        asmDumpf("  lea rdi, %d[rbp]\n", overflow_reg_offset);
    }
    asmDumpf("  mov %d[rax], rdi\n", offset);

    offset += ONE_WORD_BYTES;
    asmDumpf("  lea rdi, %d[rbp]\n", -n->parentFunc->func->args->offset);
    asmDumpf("  mov %d[rax], rdi\n", offset);
}

static void genCodeFunction(const Node *n) {
    int regargs = 0;

    if (!n)
        return;
    else if (dumpEnv.currentFunc)
        errorUnreachable();

    dumpEnv.currentFunc = n->obj;

    regargs = n->obj->func->argsCount;
    if (regargs > REG_ARGS_MAX_COUNT)
        regargs = REG_ARGS_MAX_COUNT;

    // asmDumpc('\n');
    asmDumps(".section .text.startup,\"ax\",@progbits");
    if (!n->obj->isStatic) {
        asmDumpf(".globl %.*s\n", n->obj->token->len, n->obj->token->str);
    }
    asmDumpf("%.*s:\n", n->obj->token->len, n->obj->token->str);

    // Prologue.
    asmDumps("  push rbp");
    asmDumps("  mov rbp, rsp");
    if (n->obj->func->capStackSize)
        asmDumpf("  sub rsp, %d\n", n->obj->func->capStackSize);

    // Push arguments onto stacks from registers.
    if (regargs) {
        int count = 0;
        Obj *arg = n->obj->func->args;

        for (; count < regargs; ++count, arg = arg->next) {
            int size = sizeOf(arg->type);
            char *fmt;
            switch (sizeOf(arg->type)) {
            case 8:
                fmt = "  mov %d[rbp], %s\n";
                break;
            case 4:
                fmt = "  mov DWORD PTR %d[rbp], %s\n";
                break;
            case 1:
                fmt = "  mov BYTE PTR %d[rbp], %s\n";
                break;
            default:
                errorUnreachable();
            }
            asmDumpf(fmt, -arg->offset, getReg(argRegs[count], size));
        }
    }

    if (n->obj->func->haveVaArgs && regargs < REG_ARGS_MAX_COUNT) {
        int offset = 0;
        for (Obj *arg = n->obj->func->args; arg; arg = arg->next)
            if (!arg->next)
                offset = -arg->offset;
        for (int i = regargs; i < REG_ARGS_MAX_COUNT; ++i) {
            offset += ONE_WORD_BYTES;
            asmDumpf("  mov %d[rbp], %s\n", offset, getReg(argRegs[i], ONE_WORD_BYTES));
        }
    }

    genAsm(n->body);

    // Epilogue
    if (n->obj->token->len == 4 && memcmp(n->obj->token->str, "main", 4) == 0)
        asmDumps("  mov rax, 0");
    asmDumpf(".Lreturn_%.*s:\n", n->obj->token->len, n->obj->token->str);
    asmDumps("  mov rsp, rbp");
    asmDumps("  pop rbp");
    asmDumps("  ret");
    asmDumps(".section .note.GNU-stack,\"\",@progbits");

    dumpEnv.currentFunc = NULL;
}

static void genCodeIncrement(const Node *n, int prefix) {
    if (!n)
        return;

    Node *expr = prefix ? n->rhs : n->lhs;
    genCodeLVal(expr);
    asmDumps("  pop rax");
    asmDumps("  mov rdi, rax");
    switch (sizeOf(n->type)) {
    case 8:
        asmDumps("  mov rax, [rax]");
        break;
    case 4:
        asmDumps("  mov eax, DWORD PTR [rax]");
        asmDumps("  movsx rax, eax");
        break;
    case 1:
        asmDumps("  mov al, BYTE PTR [rax]");
        asmDumps("  movsx rax, al");
        break;
    default:
        errorUnreachable();
    }

    // Postfix increment operator refers the value before increment.
    if (!prefix)
        asmDumps("  push rax");

    switch (sizeOf(n->type)) {
    case 8:
        asmDumpf("  add rax, %d\n", getAlternativeOfOneForType(n->type));
        break;
    case 4:
        asmDumpf("  add eax, %d\n", getAlternativeOfOneForType(n->type));
        asmDumps("  movsx rax, eax");
        break;
    case 1:
        asmDumpf("  add al, %d\n", getAlternativeOfOneForType(n->type));
        asmDumps("  movsx rax, al");
        break;
    default:
        errorUnreachable();
    }

    // Prefix increment operator refers the value after increment.
    if (prefix)
        asmDumps("  push rax");

    // Reflect the expression result on variable.
    switch (sizeOf(n->type)) {
    case 8:
        asmDumps("  mov [rdi], rax");
        break;
    case 4:
        asmDumps("  mov DWORD PTR [rdi], eax");
        break;
    case 1:
        asmDumps("  mov BYTE PTR [rdi], al");
        break;
    default:
        errorUnreachable();
    }
}

static void genCodeDecrement(const Node *n, int prefix) {
    if (!n)
        return;

    Node *expr = prefix ? n->rhs : n->lhs;
    genCodeLVal(expr);
    asmDumps("  pop rax");
    asmDumps("  mov rdi, rax");
    switch (sizeOf(n->type)) {
    case 8:
        asmDumps("  mov rax, [rax]");
        break;
    case 4:
        asmDumps("  mov eax, DWORD PTR [rax]");
        asmDumps("  movsx rax, eax");
        break;
    case 1:
        asmDumps("  mov al, BYTE PTR [rax]");
        asmDumps("  movsx rax, al");
        break;
    default:
        errorUnreachable();
    }

    // Postfix decrement operator refers the value before decrement.
    if (!prefix)
        asmDumps("  push rax");

    switch (sizeOf(n->type)) {
    case 8:
        asmDumpf("  sub rax, %d\n", getAlternativeOfOneForType(n->type));
        break;
    case 4:
        asmDumpf("  sub eax, %d\n", getAlternativeOfOneForType(n->type));
        asmDumps("  movsx rax, eax");
        break;
    case 1:
        asmDumpf("  sub al, %d\n", getAlternativeOfOneForType(n->type));
        asmDumps("  movsx rax, al");
        break;
    default:
        errorUnreachable();
    }

    // Prefix decrement operator refers the value after decrement.
    if (prefix)
        asmDumps("  push rax");

    // Reflect the expression result on variable.
    switch (sizeOf(n->type)) {
    case 8:
        asmDumps("  mov [rdi], rax");
        break;
    case 4:
        asmDumps("  mov DWORD PTR [rdi], eax");
        break;
    case 1:
        asmDumps("  mov BYTE PTR [rdi], al");
        break;
    default:
        errorUnreachable();
    }
}

static void genCodeAdd(const Node *n) {
    if (!n)
        return;

    int altOne = getAlternativeOfOneForType(n->type);
    genAsm(n->lhs);
    genAsm(n->rhs);

    if (isWorkLikePointer(n->lhs->type) || isWorkLikePointer(n->rhs->type)) {
        // Load integer to RAX and pointer to RDI in either case.
        if (isWorkLikePointer(n->lhs->type)) { // ptr + num
            asmDumps("  pop rax");
            asmDumps("  pop rdi");
        } else { // num + ptr
            asmDumps("  pop rdi");
            asmDumps("  pop rax");
        }
        asmDumpf("  mov rsi, %d\n", altOne);
        asmDumps("  imul rax, rsi");
        asmDumps("  add rax, rdi");
        asmDumps("  push rax");
    } else {
        asmDumps("  pop rdi");
        asmDumps("  pop rax");
        switch (sizeOf(n->lhs->type)) {
        case 8:
            asmDumps("  add rax, rdi");
            break;
        case 4:
            asmDumps("  add eax, edi");
            asmDumps("  movsx rax, eax");
            break;
        case 1:
            asmDumps("  add al, dil");
            asmDumps("  movsx rax, al");
            break;
        default:
            errorUnreachable();
        }
        asmDumps("  push rax");
    }
}

static void genCodeSub(const Node *n) {
    if (!n)
        return;

    genAsm(n->lhs);
    genAsm(n->rhs);

    asmDumps("  pop rdi");
    asmDumps("  pop rax");
    if (n->lhs->type->type == TypePointer) {
        int altOne = getAlternativeOfOneForType(n->lhs->type);
        int subBetweenPtr = n->type->type == TypePtrdiff_t;
        if (!subBetweenPtr) {
            asmDumpf("  mov rsi, %d\n", altOne);
            asmDumps("  imul rdi, rsi");
        }
        asmDumps("  sub rax, rdi");
        if (subBetweenPtr) {
            asmDumpf("  mov rsi, %d\n", altOne);
            asmDumps("  cqo");
            asmDumps("  idiv rsi");
        }
        asmDumps("  push rax");
    } else {
        // Subtraction between arithmetic types.
        // It should be that lhs, rhs, and result have all the same type.
        switch (sizeOf(n->type)) {
        case 8:
            asmDumps("  sub rax, rdi");
            break;
        case 4:
            asmDumps("  sub eax, edi");
            asmDumps("  movsx rax, eax");
            break;
        case 1:
            asmDumps("  sub al, dil");
            asmDumps("  movsx rax, al");
            break;
        default:
            errorUnreachable();
        }
        asmDumps("  push rax");
    }
}

static void genCodeInitVarArray(const Node *n, TypeInfo *varType) {
    if (!n)
        return;

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

                genCodeInitVarArray(&initNode, varType->baseType);
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

                genCodeAssign(&initNode);
                asmDumps("  pop rax");
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

        genCodeInitVar(&init, deref.type);
    }
}

static void genCodeInitVarStruct(const Node *n, TypeInfo *varType) {
    if (varType->type != TypeStruct)
        errorUnreachable();

    Node *var = n->lhs;
    if (n->rhs->type->type == TypeStruct) {
        Node initNode = *n;
        initNode.kind = NodeAssignStruct;
        initNode.type = n->lhs->type;
        genAsm(&initNode);
        asmDumps("  pop rax");
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

            genAsm(&initNode);
        }
    }
}

static void genCodeInitVar(const Node *n, TypeInfo *varType) {
    if (varType->type == TypeArray) {
        genCodeInitVarArray(n, varType);
    } else if (varType->type == TypeStruct) {
        genCodeInitVarStruct(n, varType);
    } else {
        // TODO: Support this: char *str = "...";
        Node copy = *n;
        copy.kind = NodeAssign;
        copy.type = varType;
        genCodeAssign(&copy);
        asmDumps("  pop rax");
    }
}

void genAsm(const Node *n) {
    if (!n)
        return;

    if (n->kind == NodeNop) {
        // Do nothing.
    } else if (n->kind == NodeInitVar) {
        genCodeInitVar(n, n->lhs->type);
    } else if (n->kind == NodeClearStack) {
        int entire, rest;
        entire = rest = sizeOf(n->rhs->type);
        asmDumps("  mov rax, rbp");
        asmDumpf("  sub rax, %d\n", n->rhs->obj->offset);
        while (rest) {
            if (rest >= 8) {
                asmDumpf("  mov QWORD PTR %d[rax], 0\n", entire - rest);
                rest -= 8;
            } else if (rest >= 4) {
                asmDumpf("  mov DWORD PTR %d[rax], 0\n", entire - rest);
                rest -= 4;
            } else {
                asmDumpf("  mov BYTE PTR %d[rax], 0\n", entire - rest);
                rest -= 1;
            }
        }
    } else if (n->kind == NodeTypeCast) {
        int destSize;
        destSize = sizeOf(n->type);

        genAsm(n->rhs);

        // Currently, cast is needed only when destSize < 8.
        if (destSize >= 8)
            return;

        asmDumps("  pop rax");
        switch (destSize) {
        case 4:
            asmDumps("  movsx rax, eax"); // We only have signed variables yet.
            break;
        case 1:
            asmDumps("  movsx rax, al"); // We only have signed variable yet.
            break;
        default:
            errorUnreachable();
        }
        asmDumps("  push rax");
    } else if (n->kind == NodeAddress) {
        genCodeLVal(n->rhs);
    } else if (n->kind == NodeDeref) {
        genCodeDeref(n);
    } else if (n->kind == NodeBlock) {
        for (Node *c = n->body; c; c = c->next) {
            genAsm(c);
            // Statement lefts a value on the top of the stack, and it should
            // be thrown away. (But Block node does not put any value, so do
            // not pop value.)
            if (isExprNode(c)) {
                asmDumps("  pop rax");
            }
        }
    } else if (n->kind == NodeExprList) {
        if (!n->body) {
            asmDumps("  push 0  /* Represents NOP */");
            return;
        }
        for (Node *c = n->body; c; c = c->next) {
            genAsm(c);
            // Throw away values that expressions left on stack, but the last
            // expression is the exception and its result value may be used in
            // next statement.
            if (isExprNode(c) && c->next)
                asmDumps("  pop rax");
        }
    } else if (n->kind == NodeInitList) {
        errorUnreachable();
    } else if (n->kind == NodeNot) {
        // TODO: Make sure n->rhs lefts a value on stack
        genAsm(n->rhs);
        asmDumps("  pop rax");
        asmDumps("  cmp rax, 0");
        asmDumps("  sete al");
        asmDumps("  movzb rax, al");
        asmDumps("  push rax");
    } else if (n->kind == NodeLogicalAND) {
        // TODO: Make sure n->rhs lefts a value on stack
        genAsm(n->lhs);
        asmDumps("  pop rax");
        asmDumps("  cmp rax, 0");
        asmDumpf("  je .Llogicaland%d\n", n->blockID);
        genAsm(n->rhs);
        asmDumps("  pop rax");
        asmDumps("  cmp rax, 0");
        asmDumpf(".Llogicaland%d:\n", n->blockID);
        asmDumps("  setne al");
        asmDumps("  movzb rax, al");
        asmDumps("  push rax");
    } else if (n->kind == NodeLogicalOR) {
        genAsm(n->lhs);
        asmDumps("  pop rax");
        asmDumps("  cmp rax, 0");
        asmDumpf("  jne .Llogicalor%d\n", n->blockID);
        genAsm(n->rhs);
        asmDumps("  pop rax");
        asmDumps("  cmp rax, 0");
        asmDumpf(".Llogicalor%d:\n", n->blockID);
        asmDumps("  setne al");
        asmDumps("  movzb rax, al");
        asmDumps("  push rax");
    } else if (n->kind == NodeArithShiftL || n->kind == NodeArithShiftR) {
        char *op = n->kind == NodeArithShiftL ? "sal" : "sar";
        genAsm(n->lhs);
        genAsm(n->rhs);
        asmDumps("  pop rcx");
        asmDumps("  pop rax");
        asmDumpf("  %s eax, cl\n", op);
        asmDumps("  movsx rax, eax");
        asmDumps("  push rax");
    } else if (n->kind == NodeNum) {
        asmDumpf("  push %d\n", n->val);
    } else if (n->kind == NodeLiteralString) {
        asmDumpf("  lea rax, .LiteralString%d[rip]\n", n->token->literalStr->id);
        asmDumps("  push rax");
    } else if (n->kind == NodeLVar || n->kind == NodeGVar ||
               n->kind == NodeMemberAccess) {
        // When NodeLVar appears with itself alone, it should be treated as a
        // rvalue, not a lvalue.
        genCodeLVal(n);

        // But, array, struct, and function is an exception.  It works like a
        // pointer even when it's being a rvalue.
        if (n->type->type == TypeArray || n->type->type == TypeStruct ||
                n->type->type == TypeFunction)
            return;

        // In order to change this lvalue into rvalue, push a value of a
        // variable to the top of the stack.
        asmDumps("  pop rax");
        switch (sizeOf(n->type)) {
        case 8:
            asmDumps("  mov rax, [rax]");
            break;
        case 4:
            asmDumps("  mov eax, DWORD PTR [rax]");
            asmDumps("  movsx rax, eax");
            break;
        case 1:
            asmDumps("  mov al, BYTE PTR [rax]");
            asmDumps("  movsx rax, al");
            break;
        default:
            errorUnreachable();
        }
        asmDumps("  push rax");

    } else if (n->kind == NodeAssign) {
        genCodeAssign(n);
    } else if (n->kind == NodeAssignStruct) {
        int total, rest;
        total = rest = sizeOf(n->type);
        genCodeLVal(n->lhs);
        genAsm(n->rhs);
        asmDumps("  pop rdi");
        asmDumps("  pop rax");
        while (rest) {
            if (rest >= 8) {
                asmDumpf("  mov rsi, QWORD PTR %d[rdi]\n", total - rest);
                asmDumpf("  mov QWORD PTR %d[rax], rsi\n", total - rest);
                rest -= 8;
            } else if (rest >= 4) {
                asmDumpf("  mov esi, DWORD PTR %d[rdi]\n", total - rest);
                asmDumpf("  mov DWORD PTR %d[rax], esi\n", total - rest);
                rest -= 4;
            } else {
                asmDumpf("  mov sil, BYTE PTR %d[rdi]\n", total - rest);
                asmDumpf("  mov BYTE PTR %d[rax], sil\n", total - rest);
                rest -= 1;
            }
        }
        asmDumps("  push rax");
    } else if (n->kind == NodeBreak) {
        asmDumpf("  jmp .Lend%d\n", dumpEnv.loopBlockID);
    } else if (n->kind == NodeContinue) {
        asmDumpf("  jmp .Literator%d\n", dumpEnv.loopBlockID);
    } else if (n->kind == NodeReturn) {
        genCodeReturn(n);
    } else if (n->kind == NodeConditional) {
        genAsm(n->condition);
        asmDumps("  pop rax");
        asmDumps("  cmp rax, 0");
        asmDumpf("  je .Lcond_falsy_%d\n", n->blockID);
        genAsm(n->lhs);
        asmDumpf("  jmp .Lcond_end_%d\n", n->blockID);
        asmDumpf(".Lcond_falsy_%d:\n", n->blockID);
        genAsm(n->rhs);
        asmDumpf(".Lcond_end_%d:\n", n->blockID);
    } else if (n->kind == NodeIf) {
        genCodeIf(n);
    } else if (n->kind == NodeSwitch) {
        genCodeSwitch(n);
    } else if (n->kind == NodeSwitchCase) {
        genCodeSwitchCase(n);
    } else if (n->kind == NodeFor) {
        genCodeFor(n);
    } else if (n->kind == NodeDoWhile) {
        genCodeDoWhile(n);
    } else if (n->kind == NodeFCall) {
        genCodeFCall(n);
    } else if (n->kind == NodeVaStart) {
        genCodeVaStart(n);
    } else if (n->kind == NodeFunction) {
        genCodeFunction(n);
    } else if (n->kind == NodePreIncl || n->kind == NodePostIncl) {
        genCodeIncrement(n, n->kind == NodePreIncl);
    } else if (n->kind == NodePreDecl || n->kind == NodePostDecl) {
        genCodeDecrement(n, n->kind == NodePreDecl);
    } else if (n->kind == NodeAdd) {
        genCodeAdd(n);
    } else if (n->kind == NodeSub) {
        genCodeSub(n);
    } else {
        genAsm(n->lhs);
        genAsm(n->rhs);

        asmDumps("  pop rdi");
        asmDumps("  pop rax");

        // Maybe these oprands should use only 8bytes registers.
        if (n->kind == NodeMul) {
            asmDumps("  imul rax, rdi");
        } else if (n->kind == NodeDiv) {
            asmDumps("  cqo");
            asmDumps("  idiv rdi");
        } else if (n->kind == NodeDivRem) {
            asmDumps("  cqo");
            asmDumps("  idiv rdi");
            asmDumps("  push rdx");
            return;
        } else if (n->kind == NodeEq) {
            asmDumps("  cmp rax, rdi");
            asmDumps("  sete al");
            asmDumps("  movzb rax, al");
        } else if (n->kind == NodeNeq) {
            asmDumps("  cmp rax, rdi");
            asmDumps("  setne al");
            asmDumps("  movzb rax, al");
        } else if (n->kind == NodeLT) {
            asmDumps("  cmp rax, rdi");
            asmDumps("  setl al");
            asmDumps("  movzb rax, al");
        } else if (n->kind == NodeLE) {
            asmDumps("  cmp rax, rdi");
            asmDumps("  setle al");
            asmDumps("  movzb rax, al");
        } else if (n->kind == NodeBitwiseAND) {
            asmDumps("  and rax, rdi");
        } else if (n->kind == NodeBitwiseOR) {
            asmDumps("  or rax, rdi");
        } else if (n->kind == NodeBitwiseXOR) {
            asmDumps("  xor rax, rdi");
        } else {
            errorUnreachable();
        }

        asmDumps("  push rax");
    }
}
