#ifndef __MIMICC_STDIO_H
#define __MIMICC_STDIO_H

#include <stdarg.h>
#include <stddef.h>

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

typedef struct FILE FILE;
struct FILE {};

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int fclose(FILE *stream);
FILE *fopen(const char *filename, const char *mode);
size_t fread(void *buf, size_t size, size_t n, FILE *fp);
int fseek(FILE *stream, int offset, int origin);
int ftell(FILE *stream);
int fputc(int c, FILE *stream);
int fprintf(FILE *stream, const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int printf(const char *fmt, ...);
int puts(const char *str);
int vfprintf(FILE * stream, const char *fmt, va_list args);

#endif
