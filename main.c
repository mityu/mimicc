#include <stdio.h>
#include "mimic.h"

Globals globals;


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Invalid arguments\n");
        return 1;
    }

    globals.source = argv[1];
    globals.token = tokenize();
    Node *n = expr();

    puts(".intel_syntax noprefix");
    puts(".globl main");
    puts("main:");

    genCode(n);

    puts("  pop rax");
    puts("  ret");

    return 0;
}
