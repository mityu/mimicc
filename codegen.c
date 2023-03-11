#include <stdio.h>
#include <string.h>
#include "mimic.h"

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

typedef enum {
    RAX,
    RDI,
    RSI,
    RDX,
    RCX,
    RBP,
    RSP,
    RBX,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15,
    RegCount,
} RegKind;

const int Reg8 = 3;
const int Reg16 = 2;
const int Reg32 = 1;
const int Reg64 = 0;

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

static const char *getReg(RegKind reg, int size) {
    switch (size) {
    case 1: return RegTable[reg][3];
    case 2: return RegTable[reg][2];
    case 4: return RegTable[reg][1];
    case 8: return RegTable[reg][0];
    }
    errorUnreachable();
    return "";
}

/*
static const char *argRegs[REG_ARGS_MAX_COUNT] = {
    "rdi", "rsi", "rdx", "rcx", "r8", "r9"
};
*/

static const RegKind argRegs[REG_ARGS_MAX_COUNT] = {
    RDI, RSI, RDX, RCX, R8, R9
};

static void genCodeGVarInit(GVarInit *initializer) {
    for (GVarInit *elem = initializer; elem; elem = elem->next) {
        if (elem->kind == GVarInitZero) {
            dumpf("  .zero %d\n", elem->size);
        } else if (elem->kind == GVarInitNum) {
            switch (elem->size) {
            case 8:
                dumpf("  .quad %d\n", elem->rhs->val);
                break;
            case 4:
                dumpf("  .long %d\n", elem->rhs->val);
                break;
            case 1:
                dumpf("  .byte %d\n", elem->rhs->val);
                break;
            default:
                errorUnreachable();
            }
        } else if (elem->kind == GVarInitString) {
            dumpf("  .string \"%s\"\n", elem->rhs->token->literalStr->string);
        } else if (elem->kind == GVarInitPointer) {
            if (elem->rhs->kind == NodeLiteralString) {
                dumpf("  .quad .LiteralString%d\n",
                        elem->rhs->token->literalStr->id);
            } else if (elem->rhs->kind == NodeGVar) {
                Token *token = initializer->rhs->token;
                dumpf("  .quad %.*s\n", token->len, token->str);
            } else if (elem->rhs->kind == NodeLVar) {
                dumpf("  .quad .StaticVar%d\n", elem->rhs->obj->staticVarID);
            } else if (elem->rhs->kind == NodeNum) {
                dumpf("  .long %d\n", elem->rhs->val);
            } else {
                errorUnreachable();
            }
        } else {
            errorUnreachable();
        }
    }
}

void genCodeGlobals(void) {
    if (globals.globalVars == NULL && globals.staticVars == NULL &&
            globals.strings == NULL)
        return;
    dumps("\n.data");
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
            dumpf(".LiteralString%d:\n", s->id);
            dumpf("  .string  \"%s\"\n", s->string);
        }

        safeFree(strings);
    }
    for (GVar *gvar = globals.globalVars; gvar; gvar = gvar->next) {
        Obj *v = gvar->obj;
        if (v->isExtern)
            continue;
        if (!v->isStatic) {
            dumpf(".globl %.*s\n", v->token->len, v->token->str);
        }
        dumpf("%.*s:\n", v->token->len, v->token->str);
        genCodeGVarInit(gvar->initializer);
    }
    for (GVar *v = globals.staticVars; v; v = v->next) {
        dumpf(".StaticVar%d:\n", v->obj->staticVarID);
        genCodeGVarInit(v->initializer);
    }
}

static void genCodeLVal(const Node *n) {
    if (!n)
        return;

    if (n->kind == NodeDeref) {
        genCode(n->rhs);
        // Address for variable must be on the top of the stack.
        return;
    } else if (n->kind == NodeExprList) {
        Node *expr = n->body;
        if (!expr)
            errorAt(n->token->str, "Not a lvalue");
        while (expr->next)
            expr = expr->next;
        if (!isLvalue(expr))
            errorAt(expr->token->str, "Not a lvalue");
        genCode(n);
        return;
    } else if (!isLvalue(n)) {
        errorAt(n->token->str, "Not a lvalue");
    }

    // Make sure the value on the top of the stack is the memory address to lhs
    // variable.
    // When it's local variable, calculate it on rax from the rbp(base pointer)
    // and offset, then store it on the top of the stack.  Calculate it on rax,
    // not on rbp, because rbp must NOT be changed until exiting from a
    // function.
    if (n->kind == NodeGVar || (n->kind == NodeLVar && n->obj->isExtern)) {
        dumpf("  lea rax, %.*s[rip]\n", n->token->len, n->token->str);
        dumps("  push rax");
    } else if (n->kind == NodeMemberAccess) {
        Obj *m = findStructMember(
                n->lhs->type->structDef, n->token->str, n->token->len);
        genCodeLVal(n->lhs);
        dumps("  pop rax");
        dumpf("  add rax, %d\n", m->offset);
        dumps("  push rax");
    } else if (n->kind == NodeLVar && n->obj->isStatic) {
        dumpf("  lea rax, .StaticVar%d[rip]\n", n->obj->staticVarID);
        dumps("  push rax");
    } else {
        dumps("  mov rax, rbp");
        dumpf("  sub rax, %d\n", n->obj->offset);
        dumps("  push rax");
    }
}

// Generate code dereferencing variables as rvalue.  If code for dereference as
// lvalue, use genCodeLVal() instead.
static void genCodeDeref(const Node *n) {
    if (!n)
        return;

    genCodeLVal(n);
    dumps("  mov rax, [rsp]");
    switch (sizeOf(n->type)) {
    case 8:
        dumps("  mov rax, [rax]");
        break;
    case 4:
        dumps("  mov eax, DWORD PTR [rax]");
        dumps("  movsx rax, eax");
        break;
    case 1:
        dumps("  mov al, BYTE PTR [rax]");
        dumps("  movsx rax, al");
        break;
    default:
        dumps("  lea rax, [rax]");
        break;
    }
    dumps("  mov [rsp], rax");
}

static void genCodeAssign(const Node *n) {
    if (!n)
        return;

    genCodeLVal(n->lhs);
    genCode(n->rhs);

    // Stack before assign:
    // |                     |
    // |       ......        |
    // +---------------------+
    // | Lhs memory address  | --> Load to RAX.
    // +---------------------+
    // |     Rhs value       | --> Load to RDI.
    // +---------------------+
    //
    // Stack after assign:
    // |                     |
    // |       ......        |
    // +---------------------+
    // |     Rhs value       | <-- Restore from RDI.
    // +---------------------+
    dumps("  pop rdi");
    dumps("  pop rax");
    switch (sizeOf(n->type)) {
    case 8:
        dumps("  mov [rax], rdi");
        break;
    case 4:
        dumps("  mov DWORD PTR [rax], edi");
        break;
    case 1:
        dumps("  mov BYTE PTR [rax], dil");
        break;
    default:
        errorUnreachable();
    }
    dumps("  push rdi");
}

static void genCodeReturn(const Node *n) {
    if (!n)
        return;

    if (n->lhs) {
        if (!isExprNode(n->lhs))
            errorAt(n->lhs->token->str, "Expression doesn't left value.");
        genCode(n->lhs);
        dumps("  pop rax");
    }
    dumpf("  jmp .Lreturn_%.*s\n",
            dumpEnv.currentFunc->token->len, dumpEnv.currentFunc->token->str);
}

static void genCodeIf(const Node *n) {
    if (!n)
        return;

    int elseblockCount = 0;
    genCode(n->condition);
    dumps("  pop rax");
    dumps("  cmp rax, 0");
    if (n->elseblock) {
        dumpf("  je .Lelse%d_%d\n", n->blockID, elseblockCount);
    } else {
        dumpf("  je .Lend%d\n", n->blockID);
    }
    genCode(n->body);
    if (isExprNode(n->body)) {
        dumps("  pop rax");
    }

    if (n->elseblock) {
        dumpf("  jmp .Lend%d\n", n->blockID);
    }
    if (n->elseblock) {
        for (Node *e = n->elseblock; e; e = e->next) {
            dumpf(".Lelse%d_%d:\n", n->blockID, elseblockCount);
            ++elseblockCount;
            if (e->kind == NodeElseif) {
                genCode(e->condition);
                dumps("  pop rax");
                dumps("  cmp rax, 0");
                if (e->next)
                    dumpf("  je .Lelse%d_%d\n", n->blockID, elseblockCount);
                else  // Last 'else' is omitted.
                    dumpf("  je .Lend%d\n", n->blockID);
            }
            genCode(e->body);
            dumpf("  jmp .Lend%d\n", n->blockID);
        }
    }
    dumpf(".Lend%d:\n", n->blockID);
}

static void genCodeSwitch(const Node *n) {
    int haveDefaultLabel = 0;
    int loopBlockIDSave;

    if (!n)
        return;


    loopBlockIDSave = dumpEnv.loopBlockID;
    dumpEnv.loopBlockID = n->blockID;
    genCode(n->condition);
    dumps("  pop rax");
    for (SwitchCase *c = n->cases; c; c = c->next) {
        if (!c->node->condition) {
            haveDefaultLabel = 1;
            continue;
        }
        dumpf("  mov rdi, %d\n", c->node->condition->val);
        dumps("  cmp rax, rdi");
        dumpf("  je .Lswitch_case_%d_%d\n",
                n->blockID, c->node->condition->val);
    }
    if (haveDefaultLabel)
        dumpf("  jmp .Lswitch_default_%d\n", n->blockID);
    else
        dumpf("  jmp .Lend%d\n", n->blockID);

    genCode(n->body);

    dumpf(".Lend%d:\n", n->blockID);

    dumpEnv.loopBlockID = loopBlockIDSave;
}

static void genCodeSwitchCase(const Node *n) {
    if (!n)
        return;

    if (n->condition)
        dumpf(".Lswitch_case_%d_%d:\n", n->blockID, n->condition->val);
    else
        dumpf(".Lswitch_default_%d:\n", n->blockID);
}

static void genCodeFor(const Node *n) {
    if (!n)
        return;

    int loopBlockIDSave = dumpEnv.loopBlockID;
    dumpEnv.loopBlockID = n->blockID;
    if (n->initializer) {
        genCode(n->initializer);

        // Not always initializer statement left a value on stack.  E.g.
        // Variable declarations won't left values on stack.
        if (isExprNode(n->initializer))
            dumps("  pop rax");
    }
    dumpf(".Lbegin%d:\n", n->blockID);
    if (n->condition) {
        genCode(n->condition);
    } else {
        dumps("  push 1");
    }
    dumps("  pop rax");
    dumps("  cmp rax, 0");
    dumpf("  je .Lend%d\n", n->blockID);
    genCode(n->body);
    if (isExprNode(n->body)) {
        dumps("  pop rax");
    }
    dumpf(".Literator%d:\n", n->blockID);
    if (n->iterator) {
        genCode(n->iterator);
        dumps("  pop rax");
    }
    dumpf("  jmp .Lbegin%d\n", n->blockID);
    dumpf(".Lend%d:\n", n->blockID);
    dumpEnv.loopBlockID = loopBlockIDSave;
}

static void genCodeDoWhile(const Node *n) {
    int loopBlockIDSave = dumpEnv.loopBlockID;
    if (!n)
        return;

    dumpEnv.loopBlockID = n->blockID;
    dumpf(".Lbegin%d:\n", n->blockID);

    genCode(n->body);
    if (isExprNode(n->body)) {
        dumps("  pop rax");
    }
    dumpf(".Literator%d:\n", n->blockID);
    genCode(n->condition);
    dumps("  pop rax");
    dumps("  cmp rax, 0");
    dumpf("  jne .Lbegin%d\n", n->blockID);
    dumpf(".Lend%d:\n", n->blockID);

    dumpEnv.loopBlockID = loopBlockIDSave;
}

static void genCodeFCall(const Node *n) {
    if (!n)
        return;

    static int stackAlignState = -1;  // RSP % 16
    int stackAlignStateSave = 0;
    int stackVarSize = 0;  // Size of local variables on stack.
    int stackArgSize = 0;  // Size of arguments (passed to function) on stack.
    int exCapToAlignRSP = 0; // Extra memory size to capture in order to align RSP.
    int regargs = n->fcall->argsCount;

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

    stackAlignState = (stackAlignState + stackVarSize) % 16;
    if (stackAlignState)
        exCapToAlignRSP = 16 - stackAlignState;
    stackAlignState = 0;

    if (exCapToAlignRSP)
        // Align RSP to multiple of 16.
        dumpf("  sub rsp, %d /* RSP alignment */\n", exCapToAlignRSP);

    for (Node *c = n->fcall->args; c; c = c->next)
        genCode(c);
    for (int i = 0; i < regargs; ++i)
        dumpf("  pop %s\n", getReg(argRegs[i], ONE_WORD_BYTES));

    // Set AL to count of float arguments in variadic arguments area.  This is
    // always 0 now.
    dumps("  mov al, 0");
    dumpf("  call %.*s\n", n->fcall->len, n->fcall->name);

    stackAlignState = stackAlignStateSave;
    if (exCapToAlignRSP)
        dumpf("  add rsp, %d /* RSP alignment */\n", exCapToAlignRSP);

    // Adjust RSP value when we used stack to pass arguments.
    if (stackArgSize)
        dumpf("  add rsp, %d /* Pop overflow args */\n", stackArgSize);

    dumps("  push rax");
}

static void genCodeVaStart(const Node *n) {
    Obj *lastArg = NULL;
    int offset = 0;  // Currently watching va_list member's offset
    int argsOverflows = 0;

    for (Obj *arg = n->parentFunc->args; arg; arg = arg->next)
        if (!arg->next)
            lastArg = arg;

    genCodeLVal(n->fcall->args->next);
    dumps("  pop rax");

    if (n->parentFunc->argsCount < REG_ARGS_MAX_COUNT) {
        dumpf("  mov DWORD PTR %d[rax], %d\n", offset,
                ONE_WORD_BYTES * (REG_ARGS_MAX_COUNT + 1) - lastArg->offset);
    } else {
        dumpf("  mov DWORD PTR %d[rax], %d\n",
                offset, ONE_WORD_BYTES * REG_ARGS_MAX_COUNT);
    }

    offset += sizeOf(&Types.Int);
    dumpf("  mov DWORD PTR %d[rax], %d\n", offset, REG_ARGS_MAX_COUNT * ONE_WORD_BYTES);

    offset += sizeOf(&Types.Int);
    if (n->parentFunc->argsCount <= REG_ARGS_MAX_COUNT) {
        dumpf("  lea rdi, %d[rbp]\n", ONE_WORD_BYTES * 2);
    } else {
        int overflow_reg_offset = -lastArg->offset + ONE_WORD_BYTES;
        dumpf("  lea rdi, %d[rbp]\n", overflow_reg_offset);
    }
    dumpf("  mov %d[rax], rdi\n", offset);

    offset += ONE_WORD_BYTES;
    if (n->parentFunc->argsCount >= REG_ARGS_MAX_COUNT) {
        dumpf("  lea rdi, %d[rbp]\n", -n->parentFunc->args->offset);
    } else {
        dumpf("  lea rdi, %d[rbp]\n", -lastArg->offset);
    }
    dumpf("  mov %d[rax], rdi\n", offset);
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

    dumpc('\n');
    if (!n->obj->isStatic) {
        dumpf(".globl %.*s\n", n->obj->token->len, n->obj->token->str);
    }
    dumpf("%.*s:\n", n->obj->token->len, n->obj->token->str);

    // Prologue.
    dumps("  push rbp");
    dumps("  mov rbp, rsp");
    if (n->obj->func->capStackSize)
        dumpf("  sub rsp, %d\n", n->obj->func->capStackSize);

    // "main" function does not set return adress on stack, so RSP value slips
    // by 8 bytes (size of 1 word).  Adjust the difference here.
    if (n->obj->token->len == 4 && memcmp(n->obj->token->str, "main", 4) == 0)
        dumpf("  sub rsp, %d\n", ONE_WORD_BYTES);

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
            dumpf(fmt, -arg->offset, getReg(argRegs[count], size));
        }
    }

    if (n->obj->func->haveVaArgs && regargs < REG_ARGS_MAX_COUNT) {
        int offset = 0;
        for (Obj *arg = n->obj->func->args; arg; arg = arg->next)
            if (!arg->next)
                offset = -arg->offset;
        for (int i = regargs; i < REG_ARGS_MAX_COUNT; ++i) {
            offset += ONE_WORD_BYTES;
            dumpf("  mov %d[rbp], %s\n",
                    offset, getReg(argRegs[i], ONE_WORD_BYTES));
        }
    }

    genCode(n->body);

    // Epilogue
    if (n->obj->token->len == 4 && memcmp(n->obj->token->str, "main", 4) == 0)
        dumps("  mov rax, 0");
    dumpf(".Lreturn_%.*s:\n", n->obj->token->len, n->obj->token->str);
    dumps("  mov rsp, rbp");
    dumps("  pop rbp");
    dumps("  ret");

    dumpEnv.currentFunc = NULL;
}

static void genCodeIncrement(const Node *n, int prefix) {
    if (!n)
        return;

    Node *expr = prefix ? n->rhs : n->lhs;
    genCodeLVal(expr);
    dumps("  pop rax");
    dumps("  mov rdi, rax");
    switch (sizeOf(n->type)) {
    case 8:
        dumps("  mov rax, [rax]");
        break;
    case 4:
        dumps("  mov eax, DWORD PTR [rax]");
        dumps("  movsx rax, eax");
        break;
    case 1:
        dumps("  mov al, BYTE PTR [rax]");
        dumps("  movsx rax, al");
        break;
    default:
        errorUnreachable();
    }

    // Postfix increment operator refers the value before increment.
    if (!prefix)
        dumps("  push rax");

    switch (sizeOf(n->type)) {
    case 8:
        dumpf("  add rax, %d\n", getAlternativeOfOneForType(n->type));
        break;
    case 4:
        dumpf("  add eax, %d\n", getAlternativeOfOneForType(n->type));
        dumps("  movsx rax, eax");
        break;
    case 1:
        dumpf("  add al, %d\n", getAlternativeOfOneForType(n->type));
        dumps("  movsx rax, al");
        break;
    default:
        errorUnreachable();
    }

    // Prefix increment operator refers the value after increment.
    if (prefix)
        dumps("  push rax");

    // Reflect the expression result on variable.
    switch (sizeOf(n->type)) {
    case 8:
        dumps("  mov [rdi], rax");
        break;
    case 4:
        dumps("  mov DWORD PTR [rdi], eax");
        break;
    case 1:
        dumps("  mov BYTE PTR [rdi], al");
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
    dumps("  pop rax");
    dumps("  mov rdi, rax");
    switch (sizeOf(n->type)) {
    case 8:
        dumps("  mov rax, [rax]");
        break;
    case 4:
        dumps("  mov eax, DWORD PTR [rax]");
        dumps("  movsx rax, eax");
        break;
    case 1:
        dumps("  mov al, BYTE PTR [rax]");
        dumps("  movsx rax, al");
        break;
    default:
        errorUnreachable();
    }

    // Postfix decrement operator refers the value before decrement.
    if (!prefix)
        dumps("  push rax");

    switch (sizeOf(n->type)) {
    case 8:
        dumpf("  sub rax, %d\n", getAlternativeOfOneForType(n->type));
        break;
    case 4:
        dumpf("  sub eax, %d\n", getAlternativeOfOneForType(n->type));
        dumps("  movsx rax, eax");
        break;
    case 1:
        dumpf("  sub al, %d\n", getAlternativeOfOneForType(n->type));
        dumps("  movsx rax, al");
        break;
    default:
        errorUnreachable();
    }

    // Prefix decrement operator refers the value after decrement.
    if (prefix)
        dumps("  push rax");

    // Reflect the expression result on variable.
    switch (sizeOf(n->type)) {
    case 8:
        dumps("  mov [rdi], rax");
        break;
    case 4:
        dumps("  mov DWORD PTR [rdi], eax");
        break;
    case 1:
        dumps("  mov BYTE PTR [rdi], al");
        break;
    default:
        errorUnreachable();
    }
}

static void genCodeAdd(const Node *n) {
    if (!n)
        return;

    int altOne = getAlternativeOfOneForType(n->type);
    genCode(n->lhs);
    genCode(n->rhs);

    if (isWorkLikePointer(n->lhs->type) || isWorkLikePointer(n->rhs->type)) {
        // Load integer to RAX and pointer to RDI in either case.
        if (isWorkLikePointer(n->lhs->type)) { // ptr + num
            dumps("  pop rax");
            dumps("  pop rdi");
        } else {  // num + ptr
            dumps("  pop rdi");
            dumps("  pop rax");
        }
        dumpf("  mov rsi, %d\n", altOne);
        dumps("  imul rax, rsi");
        dumps("  add rax, rdi");
        dumps("  push rax");
    } else {
        dumps("  pop rdi");
        dumps("  pop rax");
        switch (sizeOf(n->lhs->type)) {
        case 8:
            dumps("  add rax, rdi");
            break;
        case 4:
            dumps("  add eax, edi");
            dumps("  movsx rax, eax");
            break;
        case 1:
            dumps("  add al, dil");
            dumps("  movsx rax, al");
            break;
        default:
            errorUnreachable();
        }
        dumps("  push rax");
    }
}

static void genCodeSub(const Node *n) {
    if (!n)
        return;

    int altOne = getAlternativeOfOneForType(n->type);
    genCode(n->lhs);
    genCode(n->rhs);

    dumps("  pop rdi");
    dumps("  pop rax");
    if (altOne != 1) {
        dumpf("  mov rsi, %d\n", altOne);
        dumps("  imul rdi, rsi");
        dumps("  sub rax, rdi");
        dumps("  push rax");
    } else {
        switch (sizeOf(n->type)) {
        case 8:
            dumps("  sub rax, rdi");
            break;
        case 4:
            dumps("  sub eax, edi");
            dumps("  movsx rax, eax");
            break;
        case 1:
            dumps("  sub al, dil");
            dumps("  movsx rax, al");
            break;
        default:
            errorUnreachable();
        }
        dumps("  push rax");
    }
}

void genCode(const Node *n) {
    if (!n)
        return;

    if (n->kind == NodeNop) {
        // Do nothing.
    } else if (n->kind == NodeClearStack) {
        int entire, rest;
        entire = rest = sizeOf(n->rhs->type);
        dumps("  mov rax, rbp");
        dumpf("  sub rax, %d\n", n->rhs->obj->offset);
        while (rest) {
            if (rest >= 8) {
                dumpf("  mov QWORD PTR %d[rax], 0\n", entire - rest);
                rest -= 8;
            } else if (rest >= 4) {
                dumpf("  mov DWORD PTR %d[rax], 0\n", entire - rest);
                rest -= 4;
            } else {
                dumpf("  mov BYTE PTR %d[rax], 0\n", entire - rest);
                rest -= 1;
            }
        }
    } else if (n->kind == NodeTypeCast) {
        int destSize;
        destSize = sizeOf(n->type);

        genCode(n->rhs);

        // Currently, cast is needed only when destSize < 8.
        if (destSize >= 8)
            return;

        dumps("  pop rax");
        switch (destSize) {
        case 4:
            dumps("  movsx rax, eax");  // We only have signed variables yet.
            break;
        case 1:
            dumps("  movsx rax, al");  // We only have signed variable yet.
            break;
        default:
            errorUnreachable();
        }
        dumps("  push rax");
    } else if (n->kind == NodeAddress) {
        genCodeLVal(n->rhs);
    } else if (n->kind == NodeDeref) {
        genCodeDeref(n);
    } else if (n->kind == NodeBlock) {
        for (Node *c = n->body; c; c = c->next) {
            genCode(c);
            // Statement lefts a value on the top of the stack, and it should
            // be thrown away. (But Block node does not put any value, so do
            // not pop value.)
            if (isExprNode(c)) {
                dumps("  pop rax");
            }
        }
    } else if (n->kind == NodeExprList) {
        if (!n->body) {
            dumps("  push 0  /* Represents NOP */");
            return;
        }
        for (Node *c = n->body; c; c = c->next) {
            genCode(c);
            // Throw away values that expressions left on stack, but the last
            // expression is the exception and its result value may be used in
            // next statement.
            if (isExprNode(c) && c->next)
                dumps("  pop rax");
        }
    } else if (n->kind == NodeInitList) {
        errorUnreachable();
    } else if (n->kind == NodeNot) {
        // TODO: Make sure n->rhs lefts a value on stack
        genCode(n->rhs);
        dumps("  pop rax");
        dumps("  cmp rax, 0");
        dumps("  sete al");
        dumps("  movzb rax, al");
        dumps("  push rax");
    } else if (n->kind == NodeLogicalAND) {
        // TODO: Make sure n->rhs lefts a value on stack
        genCode(n->lhs);
        dumps("  pop rax");
        dumps("  cmp rax, 0");
        dumpf("  je .Llogicaland%d\n", n->blockID);
        genCode(n->rhs);
        dumps("  pop rax");
        dumps("  cmp rax, 0");
        dumpf(".Llogicaland%d:\n", n->blockID);
        dumps("  setne al");
        dumps("  movzb rax, al");
        dumps("  push rax");
    } else if (n->kind == NodeLogicalOR) {
        genCode(n->lhs);
        dumps("  pop rax");
        dumps("  cmp rax, 0");
        dumpf("  jne .Llogicalor%d\n", n->blockID);
        genCode(n->rhs);
        dumps("  pop rax");
        dumps("  cmp rax, 0");
        dumpf(".Llogicalor%d:\n", n->blockID);
        dumps("  setne al");
        dumps("  movzb rax, al");
        dumps("  push rax");
    } else if (n->kind == NodeNum) {
        dumpf("  push %d\n", n->val);
    } else if (n->kind == NodeLiteralString) {
        dumpf("  lea rax, .LiteralString%d[rip]\n", n->token->literalStr->id);
        dumps("  push rax");
    } else if (n->kind == NodeLVar || n->kind == NodeGVar ||
            n->kind == NodeMemberAccess) {
        // When NodeLVar appears with itself alone, it should be treated as a
        // rvalue, not a lvalue.
        genCodeLVal(n);

        // But, array and struct is an exception.  It works like a pointer even
        // when it's being a rvalue.
        if (n->type->type == TypeArray || n->type->type == TypeStruct)
            return;

        // In order to change this lvalue into rvalue, push a value of a
        // variable to the top of the stack.
        dumps("  pop rax");
        switch (sizeOf(n->type)) {
        case 8:
            dumps("  mov rax, [rax]");
            break;
        case 4:
            dumps("  mov eax, DWORD PTR [rax]");
            dumps("  movsx rax, eax");
            break;
        case 1:
            dumps("  mov al, BYTE PTR [rax]");
            dumps("  movsx rax, al");
            break;
        default:
            errorUnreachable();
        }
        dumps("  push rax");

    } else if (n->kind == NodeAssign) {
        genCodeAssign(n);
    } else if (n->kind == NodeAssignStruct) {
        int total, rest;
        total = rest = sizeOf(n->type);
        genCodeLVal(n->lhs);
        genCode(n->rhs);
        dumps("  pop rdi");
        dumps("  pop rax");
        while (rest) {
            if (rest >= 8) {
                dumpf("  mov rsi, QWORD PTR %d[rdi]\n", total - rest);
                dumpf("  mov QWORD PTR %d[rax], rsi\n", total - rest);
                rest -= 8;
            } else if (rest >= 4) {
                dumpf("  mov esi, DWORD PTR %d[rdi]\n", total - rest);
                dumpf("  mov DWORD PTR %d[rax], esi\n", total - rest);
                rest -= 4;
            } else {
                dumpf("  mov sil, BYTE PTR %d[rdi]\n", total - rest);
                dumpf("  mov BYTE PTR %d[rax], sil\n", total - rest);
                rest -= 1;
            }
        }
        dumps("  push rax");
    } else if (n->kind == NodeBreak) {
        dumpf("  jmp .Lend%d\n", dumpEnv.loopBlockID);
    } else if (n->kind == NodeContinue) {
        dumpf("  jmp .Literator%d\n", dumpEnv.loopBlockID);
    } else if (n->kind == NodeReturn) {
        genCodeReturn(n);
    } else if (n->kind == NodeConditional) {
        genCode(n->condition);
        dumps("  pop rax");
        dumps("  cmp rax, 0");
        dumpf("  je .Lcond_falsy_%d\n", n->blockID);
        genCode(n->lhs);
        dumpf("  jmp .Lcond_end_%d\n", n->blockID);
        dumpf(".Lcond_falsy_%d:\n", n->blockID);
        genCode(n->rhs);
        dumpf(".Lcond_end_%d:\n", n->blockID);
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
        genCode(n->lhs);
        genCode(n->rhs);

        dumps("  pop rdi");
        dumps("  pop rax");

        // Maybe these oprands should use only 8bytes registers.
        if (n->kind == NodeMul) {
            dumps("  imul rax, rdi");
        } else if (n->kind == NodeDiv) {
            dumps("  cqo");
            dumps("  idiv rdi");
        } else if (n->kind == NodeDivRem) {
            dumps("  cqo");
            dumps("  idiv rdi");
            dumps("  push rdx");
            return;
        } else if (n->kind == NodeEq) {
            dumps("  cmp rax, rdi");
            dumps("  sete al");
            dumps("  movzb rax, al");
        } else if (n->kind == NodeNeq) {
            dumps("  cmp rax, rdi");
            dumps("  setne al");
            dumps("  movzb rax, al");
        } else if (n->kind == NodeLT) {
            dumps("  cmp rax, rdi");
            dumps("  setl al");
            dumps("  movzb rax, al");
        } else if (n->kind == NodeLE) {
            dumps("  cmp rax, rdi");
            dumps("  setle al");
            dumps("  movzb rax, al");
        }

        dumps("  push rax");
    }
}
