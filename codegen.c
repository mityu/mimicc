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
//
// Equivalents for some control statements:
//  - if statement
//      if (A)
//        B
//
//      if (A == 0)
//         goto Lend;
//         B
//      Lend:
//
//  - if-else statement
//      if (A) B else C
//
//      if (A == 0)
//          goto Lelse;
//          B
//          goto Lend;
//      Lelse:
//          C
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
        return;
    } else if (n->kind == NodeReturn) {
        genCode(n->lhs);
        puts("  pop rax");
        puts("  mov rsp, rbp");
        puts("  pop rbp");
        puts("  ret");
        return;
    } else if (n->kind == NodeIf) {
        genCode(n->lhs);  // Compile condition.
        puts("  pop rax");
        puts("  cmp rax, 0");
        printf("  je .Lend%d\n", n->blockID);
        genCode(n->rhs); // Compile body.
        printf(".Lend%d:\n", n->blockID);
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
