#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "mimic.h"

struct Types Types;
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

// Print error message on stderr and exit.
void error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

_Noreturn void errorAt(char *loc, const char *fmt, ...) {
    char *head = loc;  // Head of error line
    char *tail = loc;  // Tail of error line
    int linenr = 1;    // Line number of error line
    int indent = 0;
    int pos = 0;
    va_list ap;
    va_start(ap, fmt);

    // Search for line head and tail.
    while (head > globals.source && head[-1] != '\n')
        head--;
    while (*tail != '\n')
        tail++;

    for (char *p = globals.source; p < head; ++p)
        if (*p == '\n')
            linenr++;

    indent = fprintf(stderr, "%s:%d: ", globals.sourceFile, linenr);
    fprintf(stderr, "%.*s\n", (int)(tail - head), head);

    pos = loc - head + indent;
    if (pos) {
        fprintf(stderr, "%*s", pos, " ");
    }
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
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
    if (argc != 2) {
        fprintf(stderr, "Invalid arguments\n");
        return 1;
    }

#define PrimitiveType(type) (TypeInfo){NULL, type, NULL, 0, NULL, NULL, NULL}
    Types.None   = PrimitiveType(TypeNone),
    Types.Void   = PrimitiveType(TypeVoid),
    Types.Int    = PrimitiveType(TypeInt),
    Types.Char   = PrimitiveType(TypeChar),
    Types.Number = PrimitiveType(TypeNumber),
#undef PrimitiveType

    memset(&globals, 0, sizeof(globals));
    globals.sourceFile = argv[1];
    globals.source = readFile(globals.sourceFile);
    globals.token = tokenize();
    program();

    verifyType(globals.code);
    verifyFlow(globals.code);

    puts(".intel_syntax noprefix");
    genCode(globals.code);
    genCodeGlobals();

    return 0;
}
