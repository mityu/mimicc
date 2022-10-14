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

static void genCodeDeref(Node *n);

static void printn(const char* str) {
    for (; *str; ++str)
        putchar(*str);
}

static void printlen(const char* str, int len) {
    for (int i = 0; i < len; ++i)
        putchar(str[i]);
}

static const char *argRegs[REG_ARGS_MAX_COUNT] = {
    "rdi", "rsi", "rdx", "rcx", "r8", "r9"
};

static void genCodeLVal(Node *n) {
    if (n->kind == NodeDeref) {
        genCodeDeref(n);
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

static void genCodeDeref(Node *n) {
    genCodeLVal(n->rhs);
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
    }
    printf(".Lbegin%d:\n", n->blockID);
    if (n->condition) {
        genCode(n->condition);
    }
    puts("  pop rax");
    puts("  cmp rax, 0");
    printf("  je .Lend%d\n", n->blockID);
    genCode(n->body);
    if (n->iterator) {
        genCode(n->iterator);
    }
    printf("  jmp .Lbegin%d\n", n->blockID);
    printf(".Lend%d:\n", n->blockID);
}

static void genCodeFCall(Node *n) {
    int regargs = n->fcall->argsCount;
    if (regargs > REG_ARGS_MAX_COUNT)
        regargs = REG_ARGS_MAX_COUNT;
    for (Node *c = n->fcall->args; c; c = c->next)
        genCode(c);
    for (int i = 0; i < regargs; ++i)
        printf("  pop %s\n", argRegs[i]);

    printf("  call ");
    for (int i = 0; i < n->fcall->len; ++i) {
        putchar(n->fcall->name[i]);
    }
    putchar('\n');

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
    printf("  sub rsp, %d\n", regargs * 8);
    for (int i = 0; i < regargs; ++i) {
        printf("  mov %d[rbp], %s\n", -((i + 1) * 8), argRegs[i]);
    }

    genCode(n->body);

    // Epilogue
    puts("  mov rsp, rbp");
    puts("  pop rbp");
}

void genCode(Node *n) {
    if (n->kind == NodeAddress) {
        genCodeLVal(n->rhs);
        return;
    } else if (n->kind == NodeDeref) {
        genCodeDeref(n);
        return;
    } else if (n->kind == NodeBlock) {
        for (Node *c = n->body; c; c = c->next) {
            if (n->localVarCount)
                printf("  sub rsp, %d\n", n->localVarCount * 8);
            genCode(c);
            // Statement lefts a value on the top of the stack, and it should
            // be thrown away.
            puts("  pop rax");
        }
        return;
    } else if (n->kind == NodeNum) {
        printf("  push %d\n", n->val);
        return;
    } else if (n->kind == NodeLVar) {
        // When NodeLVar appears with itself alone, it should be treated as a
        // rvalue, not a lvalue.
        genCodeLVal(n);

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
    }

    genCode(n->lhs);
    genCode(n->rhs);

    puts("  pop rdi");
    puts("  pop rax");

    if (n->kind == NodeAdd) {
        puts("  add rax, rdi");
    } else if (n->kind == NodeSub) {
        puts("  sub rax, rdi");
    } else if (n->kind == NodeMul) {
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
