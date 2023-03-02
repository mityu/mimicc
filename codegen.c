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

static int loopBlockID = 0;

static void printn(const char* str) {
    for (; *str; ++str)
        putchar(*str);
}

static void printlen(const char* str, int len) {
    for (int i = 0; i < len; ++i)
        putchar(str[i]);
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
    case NodeLiteralString:
    case NodeLVar:
    case NodeAssign:
    case NodeFCall:
    case NodeExprList:
    case NodeGVar:
        return 1;
    case NodeIf:
    case NodeElseif:
    case NodeElse:
    case NodeFor:
    case NodeDoWhile:
    case NodeBlock:
    case NodeBreak:
    case NodeContinue:
    case NodeReturn:
    case NodeFunction:
    case NodeClearStack:
        return 0;
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

void genCodeGlobals(void) {
    if (globals.globalEnv.vars == NULL && globals.strings == NULL)
        return;
    puts("\n.data");
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
            printf(".LiteralString%d:\n", s->id);
            printf("  .string  \"%s\"\n", s->string);
        }

        safeFree(strings);
    }
    for (Obj *v = globals.globalEnv.vars; v; v = v->next) {
        printlen(v->token->str, v->token->len);
        puts(":");
        printf("  .zero %d\n", sizeOf(v->type));
    }
}

static void genCodeLVal(const Node *n) {
    if (!n)
        return;

    if (n->kind == NodeDeref) {
        genCode(n->rhs);
        // Address for variable must be on the top of the stack.
        return;
    } else if (!(n->kind == NodeLVar || n->kind == NodeGVar ||
                n->kind == NodeMemberAccess)) {
        error("Lhs of assignment is not a variable.");
    }

    // Make sure the value on the top of the stack is the memory address to lhs
    // variable.
    // When it's local variable, calculate it on rax from the rbp(base pointer)
    // and offset, then store it on the top of the stack.  Calculate it on rax,
    // not on rbp, because rbp must NOT be changed until exiting from a
    // function.
    if (n->kind == NodeGVar) {
        printn("  lea rax, ");
        printlen(n->token->str, n->token->len);
        puts("[rip]");
        puts("  push rax");
    } else if (n->kind == NodeMemberAccess) {
        Obj *m = findStructMember(
                n->lhs->type->structEntity, n->token->str, n->token->len);
        genCodeLVal(n->lhs);
        puts("  pop rax");
        printf("  add rax, %d\n", m->offset);
        puts("  push rax");
    } else {
        puts("  mov rax, rbp");
        printf("  sub rax, %d\n", n->offset);
        puts("  push rax");
    }
}

// Generate code dereferencing variables as rvalue.  If code for dereference as
// lvalue, use genCodeLVal() instead.
static void genCodeDeref(const Node *n) {
    if (!n)
        return;

    genCodeLVal(n);
    puts("  mov rax, [rsp]");
    switch (sizeOf(n->type)) {
    case 8:
        puts("  mov rax, [rax]");
        break;
    case 4:
        puts("  mov eax, DWORD PTR [rax]");
        puts("  movsx rax, eax");
        break;
    case 1:
        puts("  mov al, BYTE PTR [rax]");
        puts("  movsx rax, al");
        break;
    default:
        errorUnreachable();
    }
    puts("  mov [rsp], rax");
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
    puts("  pop rdi");
    puts("  pop rax");
    switch (sizeOf(n->type)) {
    case 8:
        puts("  mov [rax], rdi");
        break;
    case 4:
        puts("  mov DWORD PTR [rax], edi");
        break;
    case 1:
        puts("  mov BYTE PTR [rax], dil");
        break;
    default:
        errorUnreachable();
    }
    puts("  push rdi");
}

static void genCodeReturn(const Node *n) {
    if (!n)
        return;

    genCode(n->lhs);
    puts("  pop rax");
    puts("  mov rsp, rbp");
    puts("  pop rbp");
    puts("  ret");
}

static void genCodeIf(const Node *n) {
    if (!n)
        return;

    int elseblockCount = 0;
    genCode(n->condition);
    puts("  pop rax");
    puts("  cmp rax, 0");
    if (n->elseblock) {
        printf("  je .Lelse%d_%d\n", n->blockID, elseblockCount);
    } else {
        printf("  je .Lend%d\n", n->blockID);
    }
    genCode(n->body);
    if (isExprNode(n->body)) {
        puts("  pop rax");
    }

    if (n->elseblock) {
        printf("  jmp .Lend%d\n", n->blockID);
    }
    if (n->elseblock) {
        for (Node *e = n->elseblock; e; e = e->next) {
            printf(".Lelse%d_%d:\n", n->blockID, elseblockCount);
            ++elseblockCount;
            if (e->kind == NodeElseif) {
                genCode(e->condition);
                puts("  pop rax");
                puts("  cmp rax, 0");
                if (e->next)
                    printf("  je .Lelse%d_%d\n", n->blockID, elseblockCount);
                else  // Last 'else' is omitted.
                    printf("  je .Lend%d\n", n->blockID);
            }
            genCode(e->body);
            printf("  jmp .Lend%d\n", n->blockID);
        }
    }
    printf(".Lend%d:\n", n->blockID);
}

static void genCodeFor(const Node *n) {
    if (!n)
        return;

    int loopBlockIDSave = loopBlockID;
    loopBlockID = n->blockID;
    if (n->initializer) {
        genCode(n->initializer);

        // Not always initializer statement left a value on stack.  E.g.
        // Variable declarations won't left values on stack.
        if (isExprNode(n->initializer))
            puts("  pop rax");
    }
    printf(".Lbegin%d:\n", n->blockID);
    if (n->condition) {
        genCode(n->condition);
    } else {
        puts("  push 1");
    }
    puts("  pop rax");
    puts("  cmp rax, 0");
    printf("  je .Lend%d\n", n->blockID);
    genCode(n->body);
    if (isExprNode(n->body)) {
        puts("  pop rax");
    }
    printf(".Literator%d:\n", n->blockID);
    if (n->iterator) {
        genCode(n->iterator);
        puts("  pop rax");
    }
    printf("  jmp .Lbegin%d\n", n->blockID);
    printf(".Lend%d:\n", n->blockID);
    loopBlockID = loopBlockIDSave;
}

static void genCodeDoWhile(const Node *n) {
    int loopBlockIDSave = loopBlockID;
    if (!n)
        return;

    loopBlockID = n->blockID;
    printf(".Lbegin%d:\n", n->blockID);
    // "continue" statement needs iterator label.
    printf(".Literator%d:\n", n->blockID);

    genCode(n->body);
    if (isExprNode(n->body)) {
        puts("  pop rax");
    }
    genCode(n->condition);
    puts("  pop rax");
    puts("  cmp rax, 0");
    printf("  jne .Lbegin%d\n", n->blockID);
    printf(".Lend%d:\n", n->blockID);

    loopBlockID = loopBlockIDSave;
}

static void genCodeFCall(const Node *n) {
    if (!n)
        return;

    static int stackAlignState = -1;  // RSP % 16
    int stackAlignStateSave = 0;
    int stackVarSize = 0; // Size of local variables on stack.
    int stackArgSize = 0;  // Size (not count) of arguments on stack.
    int exCapAlignRSP = 0; // Extra memory size to capture to align RSP.
    int regargs = n->fcall->argsCount;

    if (regargs > REG_ARGS_MAX_COUNT)
        regargs = REG_ARGS_MAX_COUNT;

    if (stackAlignState == -1) {
        stackAlignState = 0;
        for (Env *env = n->env; env; env = env->outer)
            stackVarSize += env->varSize;
    }

    if ((n->fcall->argsCount - regargs) > 0) {
        Node *arg = n->fcall->args;
        for (int i = 0; i < regargs; ++i)
            arg = arg->next;
        stackArgSize = 0;
        for (; arg; arg = arg->next) {
            stackArgSize += ONE_WORD_BYTES;
        }
        stackVarSize += stackArgSize;
    }

    stackAlignStateSave = stackAlignState;
    stackAlignState = (stackAlignState + stackVarSize) % 16;
    if (stackAlignState)
        exCapAlignRSP = 16 - stackAlignState;

    if (exCapAlignRSP)
        // Align RSP to multiple of 16.
        printf("  sub rsp, %d /* RSP alignment */\n", exCapAlignRSP);

    for (Node *c = n->fcall->args; c; c = c->next)
        genCode(c);
    for (int i = 0; i < regargs; ++i)
        printf("  pop %s\n", getReg(argRegs[i], ONE_WORD_BYTES));

    // Set AL to count of float arguments in variadic arguments area.  This is
    // always 0 now.
    puts("  mov al, 0");

    printf("  call ");
    for (int i = 0; i < n->fcall->len; ++i) {
        putchar(n->fcall->name[i]);
    }
    putchar('\n');

    stackAlignState = stackAlignStateSave;
    if (exCapAlignRSP)
        printf("  add rsp, %d /* RSP alignment */\n", exCapAlignRSP);

    // Adjust RSP value when we used stack to pass arguments.
    if (stackArgSize)
        printf("  add rsp, %d\n", stackArgSize);

    puts("  push rax");
}

static void genCodeFunction(const Node *n) {
    if (!n)
        return;

    int regargs = n->func->func->argsCount;
    if (regargs > REG_ARGS_MAX_COUNT)
        regargs = REG_ARGS_MAX_COUNT;

    putchar('\n');
    if (!n->func->is_static) {
        printn(".globl ");
        printlen(n->func->token->str, n->func->token->len);
        putchar('\n');
    }
    printlen(n->func->token->str, n->func->token->len);
    puts(":");

    // Prologue.
    puts("  push rbp");
    puts("  mov rbp, rsp");

    // Push arguments onto stacks from registers.
    if (regargs) {
        int offsets[REG_ARGS_MAX_COUNT] = {};
        int size[REG_ARGS_MAX_COUNT] = {};
        int totalOffset = 0;
        Obj *arg = n->func->func->args;

        for (int i = 0; i < regargs; ++i) {
            size[i] = sizeOf(arg->type);
            totalOffset += size[i];
            offsets[i] = totalOffset;
            arg = arg->next;
        }

        printf("  sub rsp, %d\n", totalOffset);
        for (int i = 0; i < regargs; ++i) {
            const char *fmt;
            switch (size[i]) {
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
            printf(fmt, -offsets[i], getReg(argRegs[i], size[i]));
        }
    }

    genCode(n->body);

    // Epilogue
    puts("  mov rsp, rbp");
    puts("  pop rbp");
    puts("  ret");
}

static void genCodeIncrement(const Node *n, int prefix) {
    if (!n)
        return;

    Node *expr = prefix ? n->rhs : n->lhs;
    genCodeLVal(expr);
    puts("  pop rax");
    puts("  mov rdi, rax");
    switch (sizeOf(n->type)) {
    case 8:
        puts("  mov rax, [rax]");
        break;
    case 4:
        puts("  mov eax, DWORD PTR [rax]");
        puts("  movsx rax, eax");
        break;
    case 1:
        puts("  mov al, BYTE PTR [rax]");
        puts("  movsx rax, al");
        break;
    default:
        errorUnreachable();
    }

    // Postfix increment operator refers the value before increment.
    if (!prefix)
        puts("  push rax");

    switch (sizeOf(n->type)) {
    case 8:
        printf("  add rax, %d\n", getAlternativeOfOneForType(n->type));
        break;
    case 4:
        printf("  add eax, %d\n", getAlternativeOfOneForType(n->type));
        puts("  movsx rax, eax");
        break;
    case 1:
        printf("  add al, %d\n", getAlternativeOfOneForType(n->type));
        puts("  movsx rax, al");
        break;
    default:
        errorUnreachable();
    }

    // Prefix increment operator refers the value after increment.
    if (prefix)
        puts("  push rax");

    // Reflect the expression result on variable.
    switch (sizeOf(n->type)) {
    case 8:
        puts("  mov [rdi], rax");
        break;
    case 4:
        puts("  mov DWORD PTR [rdi], eax");
        break;
    case 1:
        puts("  mov BYTE PTR [rdi], al");
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
    puts("  pop rax");
    puts("  mov rdi, rax");
    switch (sizeOf(n->type)) {
    case 8:
        puts("  mov rax, [rax]");
        break;
    case 4:
        puts("  mov eax, DWORD PTR [rax]");
        puts("  movsx rax, eax");
        break;
    case 1:
        puts("  mov al, BYTE PTR [rax]");
        puts("  movsx rax, al");
        break;
    default:
        errorUnreachable();
    }

    // Postfix decrement operator refers the value before decrement.
    if (!prefix)
        puts("  push rax");

    switch (sizeOf(n->type)) {
    case 8:
        printf("  sub rax, %d\n", getAlternativeOfOneForType(n->type));
        break;
    case 4:
        printf("  sub eax, %d\n", getAlternativeOfOneForType(n->type));
        puts("  movsx rax, eax");
        break;
    case 1:
        printf("  sub al, %d\n", getAlternativeOfOneForType(n->type));
        puts("  movsx rax, al");
        break;
    default:
        errorUnreachable();
    }

    // Prefix decrement operator refers the value after decrement.
    if (prefix)
        puts("  push rax");

    // Reflect the expression result on variable.
    switch (sizeOf(n->type)) {
    case 8:
        puts("  mov [rdi], rax");
        break;
    case 4:
        puts("  mov DWORD PTR [rdi], eax");
        break;
    case 1:
        puts("  mov BYTE PTR [rdi], al");
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
            puts("  pop rax");
            puts("  pop rdi");
        } else {  // num + ptr
            puts("  pop rdi");
            puts("  pop rax");
        }
        printf("  mov rsi, %d\n", altOne);
        puts("  imul rax, rsi");
        puts("  add rax, rdi");
        puts("  push rax");
    } else {
        puts("  pop rdi");
        puts("  pop rax");
        switch (sizeOf(n->lhs->type)) {
        case 8:
            puts("  add rax, rdi");
            break;
        case 4:
            puts("  add eax, edi");
            puts("  movsx rax, eax");
            break;
        case 1:
            puts("  add al, dil");
            puts("  movsx rax, al");
            break;
        default:
            errorUnreachable();
        }
        puts("  push rax");
    }
}

static void genCodeSub(const Node *n) {
    if (!n)
        return;

    int altOne = getAlternativeOfOneForType(n->type);
    genCode(n->lhs);
    genCode(n->rhs);

    puts("  pop rdi");
    puts("  pop rax");
    if (altOne != 1) {
        printf("  mov rsi, %d\n", altOne);
        puts("  imul rdi, rsi");
        puts("  sub rax, rdi");
        puts("  push rax");
    } else {
        switch (sizeOf(n->type)) {
        case 4:
            puts("  sub eax, edi");
            puts("  movsx rax, eax");
            break;
        case 1:
            puts("  sub al, dil");
            puts("  movsx rax, al");
        default:
            errorUnreachable();
        }
        puts("  push rax");
    }
}

void genCode(const Node *n) {
    if (!n)
        return;

    if (n->kind == NodeClearStack) {
        int entire, rest;
        entire = rest = sizeOf(n->rhs->type);
        puts("  mov rax, rbp");
        printf("  sub rax, %d\n", n->rhs->offset);
        while (rest) {
            if (rest >= 8) {
                printf("  mov QWORD PTR %d[rax], 0\n", entire - rest);
                rest -= 8;
            } else if (rest >= 4) {
                printf("  mov DWORD PTR %d[rax], 0\n", entire - rest);
                rest -= 4;
            } else {
                printf("  mov BYTE PTR %d[rax], 0\n", entire - rest);
                rest -= 1;
            }
        }
    } else if (n->kind == NodeAddress) {
        genCodeLVal(n->rhs);
    } else if (n->kind == NodeDeref) {
        genCodeDeref(n);
    } else if (n->kind == NodeBlock) {
        if (n->env->varSize)
            printf("  sub rsp, %d\n", n->env->varSize);
        for (Node *c = n->body; c; c = c->next) {
            genCode(c);
            // Statement lefts a value on the top of the stack, and it should
            // be thrown away. (But Block node does not put any value, so do
            // not pop value.)
            if (isExprNode(c)) {
                puts("  pop rax");
            }
        }
        if (n->env->varSize)
            printf("  add rsp, %d\n", n->env->varSize);
    } else if (n->kind == NodeExprList) {
        if (!n->body) {
            puts("  push 0  /* Represents NOP */");
            return;
        }
        for (Node *c = n->body; c; c = c->next) {
            genCode(c);
            // Throw away values that expressions left on stack, but the last
            // expression is the exception and its result value may be used in
            // next statement.
            if (isExprNode(c) && c->next)
                puts("  pop rax");
        }
    } else if (n->kind == NodeNot) {
        // TODO: Make sure n->rhs lefts a value on stack
        genCode(n->rhs);
        puts("  pop rax");
        puts("  cmp rax, 0");
        puts("  sete al");
        puts("  movzb rax, al");
        puts("  push rax");
    } else if (n->kind == NodeLogicalAND) {
        // TODO: Make sure n->rhs lefts a value on stack
        genCode(n->lhs);
        puts("  pop rax");
        puts("  cmp rax, 0");
        printf("  je .Llogicaland%d\n", n->blockID);
        genCode(n->rhs);
        puts("  pop rax");
        puts("  cmp rax, 0");
        printf(".Llogicaland%d:\n", n->blockID);
        puts("  setne al");
        puts("  movzb rax, al");
        puts("  push rax");
    } else if (n->kind == NodeLogicalOR) {
        genCode(n->lhs);
        puts("  pop rax");
        puts("  cmp rax, 0");
        printf("  jne .Llogicalor%d\n", n->blockID);
        genCode(n->rhs);
        puts("  pop rax");
        puts("  cmp rax, 0");
        printf(".Llogicalor%d:\n", n->blockID);
        puts("  setne al");
        puts("  movzb rax, al");
        puts("  push rax");
    } else if (n->kind == NodeNum) {
        printf("  push %d\n", n->val);
    } else if (n->kind == NodeLiteralString) {
        printf("  lea rax, .LiteralString%d[rip]\n", n->token->literalStr->id);
        puts("  push rax");
    } else if (n->kind == NodeLVar || n->kind == NodeGVar || n->kind == NodeMemberAccess) {
        // When NodeLVar appears with itself alone, it should be treated as a
        // rvalue, not a lvalue.
        genCodeLVal(n);

        // But, array is an exception.  It works like a pointer even when it's
        // being a rvalue.
        if (n->type->type == TypeArray)
            return;

        // In order to change this lvalue into rvalue, push a value of a
        // variable to the top of the stack.
        puts("  pop rax");
        switch (sizeOf(n->type)) {
        case 8:
            puts("  mov rax, [rax]");
            break;
        case 4:
            puts("  mov eax, DWORD PTR [rax]");
            puts("  movsx rax, eax");
            break;
        case 1:
            puts("  mov al, BYTE PTR [rax]");
            puts("  movsx rax, al");
            break;
        default:
            errorUnreachable();
        }
        puts("  push rax");

    } else if (n->kind == NodeAssign) {
        genCodeAssign(n);
    } else if (n->kind == NodeBreak) {
        printf("  jmp .Lend%d\n", loopBlockID);
    } else if (n->kind == NodeContinue) {
        printf("  jmp .Literator%d\n", loopBlockID);
    } else if (n->kind == NodeReturn) {
        genCodeReturn(n);
    } else if (n->kind == NodeIf) {
        genCodeIf(n);
    } else if (n->kind == NodeFor) {
        genCodeFor(n);
    } else if (n->kind == NodeDoWhile) {
        genCodeDoWhile(n);
    } else if (n->kind == NodeFCall) {
        genCodeFCall(n);
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

        puts("  pop rdi");
        puts("  pop rax");

        // Maybe these oprands should use only 8bytes registers.
        if (n->kind == NodeMul) {
            puts("  imul rax, rdi");
        } else if (n->kind == NodeDiv) {
            puts("  cqo");
            puts("  idiv rdi");
        } else if (n->kind == NodeDivRem) {
            puts("  cqo");
            puts("  idiv rdi");
            puts("  push rdx");
            return;
        } else if (n->kind == NodeEq) {
            puts("  cmp rax, rdi");
            puts("  sete al");
            puts("  movzb rax, al");
        } else if (n->kind == NodeNeq) {
            puts("  cmp rax, rdi");
            puts("  setne al");
            puts("  movzb rax, al");
        } else if (n->kind == NodeLT) {
            puts("  cmp rax, rdi");
            puts("  setl al");
            puts("  movzb rax, al");
        } else if (n->kind == NodeLE) {
            puts("  cmp rax, rdi");
            puts("  setle al");
            puts("  movzb rax, al");
        }

        puts("  push rax");
    }
}
