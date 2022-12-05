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
    globals.currentFunction = NULL;
    globals.functions = NULL;
    globals.blockCount = 0;
    globals.token = tokenize();
    program();

    verifyType(globals.code);

    puts(".intel_syntax noprefix");
    genCode(globals.code);
    genCodeGvarDecl();

    return 0;
}
