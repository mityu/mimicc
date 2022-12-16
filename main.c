#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "mimic.h"

Globals globals;

// Allocate memory and return it with entirely 0 cleared.  If allocating memory
// fails, exit program immediately.
void *safeAlloc(size_t size) {
    void *p = malloc(size);
    if (!p)
        error("Allocating memory failed.");
    memset(p, 0, size);
    return p;
}

static char *readFile(const char *path) {
    FILE *fp = fopen(path, "r");
    char *buf = NULL;
    size_t size;
    if (!fp) {
        error("File open failed: %s: %s\n", path, strerror(errno));
    }

    if (fseek(fp, 0, SEEK_END) == -1) {
        fclose(fp);
        error("%s: fseek: %s\n", path, strerror(errno));
    }
    size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) == -1) {
        fclose(fp);
        error("%s: fseek: %s\n", path, strerror(errno));
    }

    buf = (char *)safeAlloc((size + 2) * sizeof(char));
    fread(buf, size, 1, fp);
    fclose(fp);

    if (size == 0 || buf[size - 1] != '\n')
        buf[size++] = '\n';
    buf[size] = '\0';
    return buf;
}

int main(int argc, char *argv[]) {
    int lvar_count = 0;

    if (argc != 2) {
        fprintf(stderr, "Invalid arguments\n");
        return 1;
    }

    globals.sourceFile = argv[1];
    globals.source = readFile(globals.sourceFile);
    globals.currentBlock = NULL;
    globals.currentFunction = NULL;
    globals.functions = NULL;
    globals.vars = NULL;
    globals.strings = NULL;
    globals.blockCount = 0;
    globals.literalStringCount = 0;
    globals.token = tokenize();
    program();

    verifyType(globals.code);

    puts(".intel_syntax noprefix");
    genCode(globals.code);
    genCodeGlobals();

    return 0;
}
