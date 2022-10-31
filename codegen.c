#include <stdio.h>
#include <string.h>
#include "mimic.h"

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

static void printn(const char* str) {
    for (; *str; ++str)
        putchar(*str);
}

static void printlen(const char* str, int len) {
    for (int i = 0; i < len; ++i)
        putchar(str[i]);
}

static int isExprNode(Node *n) {
    switch (n->kind) {
    default:
        return 0;
    case NodeAdd:  // All cases below are fallthrough.
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
    case NodeAddress:
    case NodeDeref:
    case NodeNum:
    case NodeLVar:
    case NodeAssign:
    case NodeFCall:
        return 1;
    }
}

// Return an integer corresponding to 1 for given type.
// In concrate explanation, return
// - sizeof(type) for "type*"
// - 1 for integer.
static int getAlternativeOfOneForType(TypeInfo *ti) {
    if (ti->type == TypePointer) {
        return sizeOf(ti->baseType);
    }
    return 1;
}

static const char *argRegs[REG_ARGS_MAX_COUNT] = {
    "rdi", "rsi", "rdx", "rcx", "r8", "r9"
};

static void genCodeLVal(Node *n) {
    if (n->kind == NodeDeref) {
        genCode(n->rhs);
        // Address for variable must be on the top of the stack.
        return;
    } else if (n->kind != NodeLVar) {
        error("Lhs of assignment is not a variable.");
    }

    // Make sure the value on the top of the stack is the memory address to lhs
    // variable.  Calculate it on rax from the rbp(base pointer) and offset,
    // then store it on the top of the stack.  Calculate it on rax, not on rbp,
    // because rbp must NOT be changed until exiting from a function.
    puts("  mov rax, rbp");
    printf("  sub rax, %d\n", n->offset);
    puts("  push rax");
}

// Generate code dereferencing variables as rvalue.  If code for dereference as
// lvalue, use genCodeLVal() instead.
static void genCodeDeref(Node *n) {
    genCodeLVal(n);
    puts("  mov rax, [rsp]");
    puts("  mov rax, [rax]");
    puts("  mov [rsp], rax");
}

static void genCodeAssign(Node *n) {
    genCodeLVal(n->lhs);
    genCode(n->rhs);

    // Stack before assign:
    // |                     |
    // |       ......        |
    // +---------------------+
    // | Lhs memory address  | --> Load to rax.
    // +---------------------+
    // |     Rhs value       | --> Load to rdi.
    // +---------------------+
    //
    // Stack after assign:
    // |                     |
    // |       ......        |
    // +---------------------+
    // |     Rhs value       | <-- Restore from rdi.
    // +---------------------+
    puts("  pop rdi");
    puts("  pop rax");
    puts("  mov [rax], rdi");
    puts("  push rdi");
}

static void genCodeReturn(Node *n) {
    genCode(n->lhs);
    puts("  pop rax");
    puts("  mov rsp, rbp");
    puts("  pop rbp");
    puts("  ret");
}

static void genCodeIf(Node *n) {
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
                printf("  je .Lelse%d_%d\n", n->blockID, elseblockCount);
            }
            genCode(e->body);
            printf("  jmp .Lend%d\n", n->blockID);
        }
    }
    printf(".Lend%d:\n", n->blockID);
}

static void genCodeFor(Node *n) {
    if (n->initializer) {
        genCode(n->initializer);
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
    if (n->iterator) {
        genCode(n->iterator);
        puts("  pop rax");
    }
    printf("  jmp .Lbegin%d\n", n->blockID);
    printf(".Lend%d:\n", n->blockID);
}

static void genCodeFCall(Node *n) {
    int stackVarCount = 0;
    int needAlignRSP = 0;
    int regargs = n->fcall->argsCount;

    if (regargs > REG_ARGS_MAX_COUNT)
        regargs = REG_ARGS_MAX_COUNT;

    for (Node *c = n->outerBlock; c; c = c->next) {
        stackVarCount += c->localVarLen;
        if (c->kind == NodeFunction)
            break;
    }
    if ((n->fcall->argsCount - regargs) > 0)
        stackVarCount += n->fcall->argsCount - regargs;

    needAlignRSP = stackVarCount % 2;
    if (needAlignRSP)
        puts("  sub rsp, 8");  // Align RSP to multiple of 16.

    for (Node *c = n->fcall->args; c; c = c->next)
        genCode(c);
    for (int i = 0; i < regargs; ++i)
        printf("  pop %s\n", argRegs[i]);

    printf("  call ");
    for (int i = 0; i < n->fcall->len; ++i) {
        putchar(n->fcall->name[i]);
    }
    putchar('\n');

    if (needAlignRSP)
        puts("  add rsp, 8");

    // Adjust RSP value when we used stack to pass arguments.
    if ((n->fcall->argsCount - regargs) > 0)
        printf("  add rsp, %d\n", (n->fcall->argsCount - regargs) * 8);

    puts("  push rax");
}

static void genCodeFunction(Node *n) {
    int regargs = n->func->argsCount;
    if (regargs > REG_ARGS_MAX_COUNT)
        regargs = REG_ARGS_MAX_COUNT;

    putchar('\n');
    printn(".globl ");
    printlen(n->func->name, n->func->len);
    putchar('\n');
    printlen(n->func->name, n->func->len);
    puts(":");

    // Prologue.
    puts("  push rbp");
    puts("  mov rbp, rsp");

    // Push arguments onto stacks from registers.
    if (regargs) {
        printf("  sub rsp, %d\n", regargs * 8);
        for (int i = 0; i < regargs; ++i) {
            printf("  mov %d[rbp], %s\n", -((i + 1) * 8), argRegs[i]);
        }
    }

    genCode(n->body);

    // Epilogue
    puts("  mov rsp, rbp");
    puts("  pop rbp");
    puts("  ret");
}

static void genCodeIncrement(Node *n, int prefix) {
    Node *expr = prefix ? n->rhs : n->lhs;
    genCodeLVal(expr);
    puts("  pop rax");
    puts("  mov rdi, rax");
    puts("  mov rax, [rax]");

    // Postfix increment operator refers the value before increment.
    if (!prefix)
        puts("  push rax");

    printf("  add rax, %d\n", getAlternativeOfOneForType(n->type));

    // Prefix increment operator refers the value after increment.
    if (prefix)
        puts("  push rax");

    // Reflect the expression result on variable.
    puts("  mov [rdi], rax");
}

static void genCodeDecrement(Node *n, int prefix) {
    Node *expr = prefix ? n->rhs : n->lhs;
    genCodeLVal(expr);
    puts("  pop rax");
    puts("  mov rdi, rax");
    puts("  mov rax, [rax]");

    // Postfix decrement operator refers the value before decrement.
    if (!prefix)
        puts("  push rax");

    printf("  sub rax, %d\n", getAlternativeOfOneForType(n->type));

    // Prefix decrement operator refers the value after decrement.
    if (prefix)
        puts("  push rax");

    // Reflect the expression result on variable.
    puts("  mov [rdi], rax");
}

void genCode(Node *n) {
    if (n->kind == NodeAddress) {
        genCodeLVal(n->rhs);
        return;
    } else if (n->kind == NodeDeref) {
        genCodeDeref(n);
        return;
    } else if (n->kind == NodeBlock) {
        if (n->localVarLen)
            printf("  sub rsp, %d\n", n->localVarLen * 8);
        for (Node *c = n->body; c; c = c->next) {
            genCode(c);
            // Statement lefts a value on the top of the stack, and it should
            // be thrown away. (But Block node does not put any value, so do
            // not pop value.)
            if (isExprNode(c)) {
                puts("  pop rax");
            }
        }
        return;
    } else if (n->kind == NodeNum) {
        printf("  push %d\n", n->val);
        return;
    } else if (n->kind == NodeLVar) {
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
        puts("  mov rax, [rax]");
        puts("  push rax");

        return;
    } else if (n->kind == NodeAssign) {
        genCodeAssign(n);
        return;
    } else if (n->kind == NodeReturn) {
        genCodeReturn(n);
        return;
    } else if (n->kind == NodeIf) {
        genCodeIf(n);
        return;
    } else if (n->kind == NodeFor) {
        genCodeFor(n);
        return;
    } else if (n->kind == NodeFCall) {
        genCodeFCall(n);
        return;
    } else if (n->kind == NodeFunction) {
        genCodeFunction(n);
        return;
    } else if (n->kind == NodePreIncl || n->kind == NodePostIncl) {
        genCodeIncrement(n, n->kind == NodePreIncl);
        return;
    } else if (n->kind == NodePreDecl || n->kind == NodePostDecl) {
        genCodeDecrement(n, n->kind == NodePreDecl);
        return;
    } else if (n->kind == NodeAdd) {
        int altOne = getAlternativeOfOneForType(n->type);
        genCode(n->lhs);
        genCode(n->rhs);

        if (altOne != 1) {
            // Load integer to RAX and pointer to RDI in either case.
            if (n->lhs->type->type == TypePointer) { // ptr + num
                puts("  pop rax");
                puts("  pop rdi");
            } else {  // num + ptr
                puts("  pop rdi");
                puts("  pop rax");
            }
            printf("  mov rsi, %d\n", altOne);
            puts("  imul rax, rsi");
        } else {
            puts("  pop rdi");
            puts("  pop rax");
        }

        puts("  add rax, rdi");
        puts("  push rax");
        return;
    } else if (n->kind == NodeSub) {
        int altOne = getAlternativeOfOneForType(n->type);
        genCode(n->lhs);
        genCode(n->rhs);

        puts("  pop rdi");
        puts("  pop rax");
        if (altOne != 1) {
            printf("  mov rsi, %d\n", altOne);
            puts("  imul rdi, rsi");
        }
        puts("  sub rax, rdi");
        puts("  push rax");
        return;
    }

    genCode(n->lhs);
    genCode(n->rhs);

    puts("  pop rdi");
    puts("  pop rax");

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
