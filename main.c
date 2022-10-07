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


    puts(".intel_syntax noprefix");
    genCode(globals.code);

    return 0;
}
