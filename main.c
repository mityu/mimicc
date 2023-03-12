#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "mimicc.h"

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
    va_end(ap);
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
    va_end(ap);
    exit(1);
}

// Like putchar(), but dump character into output file.
int dumpc(int c) {
    if (!globals.destFile)
        errorUnreachable();
    return fputc(c, globals.destFile);
}

// Like puts(), but dump string into output file.
int dumps(const char *s) {
    if (!globals.destFile)
        errorUnreachable();
    return fprintf(globals.destFile, "%s\n", s);
}

// Like printf(), but dump string into output file.
int dumpf(const char *fmt, ...) {
    va_list ap;
    int retval;

    if (!globals.destFile)
        errorUnreachable();

    va_start(ap, fmt);
    retval = vfprintf(globals.destFile, fmt, ap);
    va_end(ap);
    return retval;
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

_Noreturn void cmdlineArgsError(int argc, char *argv[], int at, const char *msg) {
    int len = 0;
    for (int i = 0; i < argc; ++i) {
        fprintf(stderr, "%s ", argv[i]);
        if (i <= at)
            len += strlen(argv[i]) + 1;
    }
    fprintf(stderr, "\n%*s %s\n", len, "^", msg);
    exit(1);
}

int main(int argc, char *argv[]) {
    char *inFile = NULL;
    char *outFile = NULL;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0) {
            if ((++i) == argc)
                cmdlineArgsError(argc, argv, i, "File name must follow after \"-o\"");
            outFile = argv[i];
        } else if (strcmp(argv[i], "-S") == 0) {
            // Just ignore
        } else if (!inFile) {
            inFile = argv[i];
        } else {
            cmdlineArgsError(argc, argv, i - 1, "Invalid argument");
        }
    }

    if (!inFile)
        cmdlineArgsError(argc, argv, argc, "No input file is specified");
    else if (!outFile)
        cmdlineArgsError(argc, argv, argc, "No output file is specified");

#define PrimitiveType(type) (TypeInfo){NULL, type, NULL, 0, NULL, NULL, NULL}
    Types.None   = PrimitiveType(TypeNone),
    Types.Void   = PrimitiveType(TypeVoid),
    Types.Int    = PrimitiveType(TypeInt),
    Types.Char   = PrimitiveType(TypeChar),
    Types.Number = PrimitiveType(TypeNumber),
#undef PrimitiveType

    memset(&globals, 0, sizeof(globals));
    globals.currentEnv = &globals.globalEnv;
    globals.sourceFile = inFile;
    globals.source = readFile(globals.sourceFile);
    globals.token = tokenize();
    globals.token = preprocess(globals.token);
    globals.token = removeAllNewLineToken(globals.token);
    program();

    verifyType(globals.code);
    verifyFlow(globals.code);

    globals.destFile = fopen(outFile, "wb");
    if (!globals.destFile)
        error("Failed to open file: %s", outFile);

    dumps(".intel_syntax noprefix");
    genCode(globals.code);
    genCodeGlobals();

    fclose(globals.destFile);

    return 0;
}
