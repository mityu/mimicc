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
    program();

    puts(".intel_syntax noprefix");
    puts(".globl main");
    puts("main:");

    // Generate prologue.
    puts("  push rbp");
    puts("  mov rbp, rsp");
    puts("  sub rsp, 208");  // 26 * 8 = 208; reserve memory for variables.

    for (int i = 0; globals.code[i] != NULL; i++) {
        genCode(globals.code[i]);

        // Remove a "value" on the top of the stack. It's no longer useless.
        puts("  pop rax");
    }

    // Generate epilogue.
    puts("  mov rsp, rbp");
    puts("  pop rbp");

    puts("  ret");

    return 0;
}
