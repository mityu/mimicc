#include <stdio.h>
#include "mimic.h"

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
    if (n->kind == NodeNum) {
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
    }

    puts("  push rax");
}
