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
    globals.currentBlock = NULL;
    globals.blockCount = 0;
    globals.token = tokenize();
    program();

    // for (LVar *v = globals.locals; v != NULL; v = v->next)
    //     ++lvar_count;


    // if (lvar_count) {
    //     printf("  sub rsp, %d\n", lvar_count * 8);  // Reserve memories for local variables.
    // }

    puts(".intel_syntax noprefix");
    genCode(globals.code);

    return 0;
}
