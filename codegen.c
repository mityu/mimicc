#include <stdio.h>
#include <string.h>
#include "mimic.h"

#define REG_ARGS_MAX_COUNT  (6)

// "push" and "pop" operator implicitly uses rsp as memory adress.
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

static void genCodeLVal(Node *n) {
    if (n->kind != NodeLVar) {
        error("Lhs of assignment is not a variable.");
    }

    // Make sure the value of the top of the stack is the memory adress to lhs
    // variable.  Calculate it on rax from the rbp(base pointer) and offset,
    // then store it on the top of the stack.  Calculate it on rax, not on rbp,
    // because rbp must NOT be changed until exiting from a function.
    puts("  mov rax, rbp");
    printf("  sub rax, %d\n", n->offset);
    puts("  push rax");
}

void genCode(Node *n) {
    if (n->kind == NodeBlock) {
        for (Node *c = n->body; c; c = c->next) {
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
        genCodeLVal(n->lhs);
        genCode(n->rhs);

        // Stack before assign:
        // |                     |
        // |       ......        |
        // +---------------------+
        // |  Lhs memory adress  | --> Load to rax.
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
        return;
    } else if (n->kind == NodeReturn) {
        genCode(n->lhs);
        puts("  pop rax");
        puts("  mov rsp, rbp");
        puts("  pop rbp");
        puts("  ret");
        return;
    } else if (n->kind == NodeIf) {
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
        return;
    } else if (n->kind == NodeFor) {
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
        return;
    } else if (n->kind == NodeFCall) {
        static const char *regs[REG_ARGS_MAX_COUNT] = {
            "rdi", "rsi", "rdx", "rcx", "r8", "r9"
        };
        int regargs = n->fcall->argsCount;
        if (regargs > REG_ARGS_MAX_COUNT)
            regargs = REG_ARGS_MAX_COUNT;
        for (Node *c = n->fcall->args; c; c = c->next)
            genCode(c);
        for (int i = 0; i < regargs; ++i)
            printf("  pop %s\n", regs[i]);

        printf("  call ");
        for (int i = 0; i < n->fcall->len; ++i) {
            putchar(n->fcall->name[i]);
        }
        putchar('\n');

        // Adjust RSP value when we used stack to pass arguments.
        if ((n->fcall->argsCount - regargs) > 0)
            printf("  add rsp, %d\n", (n->fcall->argsCount - regargs) * 8);

        puts("  push rax");
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
