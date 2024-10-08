#include "mimicc.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

_Noreturn void errorAt(Token *loc, const char *fmt, ...) {
    char *head = NULL; // Head of error line
    char *tail = NULL; // Tail of error line
    int indent = 0;
    va_list ap;

    if (!loc) {
        fputs("Error while processing internal node: ", stderr);
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");
        va_end(ap);
        exit(1);
    }

    va_start(ap, fmt);

    head = (char *)(loc->str - loc->column);
    tail = loc->str;
    while (*tail != '\n')
        tail++;

    indent = fprintf(stderr, "%s:%d: ", loc->file->display, loc->line);
    fprintf(stderr, "%.*s\n", (int)(tail - head), head);

    if (loc->column + indent) {
        fprintf(stderr, "%*s", loc->column + indent, " ");
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

FilePath *analyzeFilepath(const char *path, const char *display) {
    FilePath *obj = (FilePath *)safeAlloc(sizeof(FilePath));
    int pathSize = 0;
    int displaySize = 0;
    int dirnameLen = 0;

    displaySize = strlen(display) + 1;
    obj->display = (char *)safeAlloc(displaySize);
    memcpy(obj->display, display, displaySize);

    pathSize = strlen(path) + 1;
    obj->path = (char *)safeAlloc(pathSize);
    memcpy(obj->path, path, pathSize);

    for (dirnameLen = pathSize; dirnameLen > 0; --dirnameLen) {
        if (path[dirnameLen - 1] == '/')
            break;
    }

    obj->dirname = (char *)safeAlloc(dirnameLen + 1);
    memcpy(obj->dirname, path, dirnameLen);
    obj->dirname[dirnameLen] = '\0';

    obj->basename = (char *)safeAlloc(pathSize - dirnameLen);
    memcpy(obj->basename, &path[dirnameLen], pathSize - dirnameLen);

    return obj;
}

char *readFile(const char *path) {
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
    char *source = NULL;

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

#define PrimitiveType(type)                                                              \
    (TypeInfo) { NULL, type }
    Types.None = PrimitiveType(TypeNone);
    Types.Void = PrimitiveType(TypeVoid);
    Types.Int = PrimitiveType(TypeInt);
    Types.Char = PrimitiveType(TypeChar);
    Types.Number = PrimitiveType(TypeNumber);
#undef PrimitiveType

    memset(&globals, 0, sizeof(globals));
    globals.currentEnv = &globals.globalEnv;
    globals.ccFile = analyzeFilepath(argv[0], argv[0]);

    globals.includePath = (char *)safeAlloc(strlen(globals.ccFile->dirname) + 9);
    sprintf(globals.includePath, "%sinclude/", globals.ccFile->dirname);

    source = readFile(inFile);
    globals.token = tokenize(source, analyzeFilepath(inFile, inFile));
    preprocess(globals.token);
    removeAllNewLineToken(globals.token);
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
