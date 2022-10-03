#include <stdio.h>
#include "mimic.h"

void genCode(Node *n) {
    if (n->kind == NodeNum) {
        printf("  push %d\n", n->val);
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
    }

    puts("  push rax");
}
