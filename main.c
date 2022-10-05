#include <stdio.h>
#include "mimic.h"

Globals globals;


int main(int argc, char *argv[]) {
    int lvar_count = 0;

    if (argc != 2) {
        fprintf(stderr, "Invalid arguments\n");
        return 1;
    }

    globals.source = argv[1];
    globals.locals = NULL;
    globals.token = tokenize();
    program();

    for (LVar *v = globals.locals; v != NULL; v = v->next)
        ++lvar_count;

    puts(".intel_syntax noprefix");
    puts(".globl main");
    puts("main:");

    // Generate prologue.
    puts("  push rbp");
    puts("  mov rbp, rsp");
    if (lvar_count) {
        printf("  sub rsp, %d\n", lvar_count * 8);  // Reserve memories for local variables.
    }

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
