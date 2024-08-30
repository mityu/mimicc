#ifndef __MIMICC_STDIO_H
#define __MIMICC_STDIO_H

#include <stdarg.h>
#include <stddef.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

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
int feof(FILE *stream);
int ferror(FILE *stream);
int fflush(FILE *stream);
int fgetc(FILE *stream);
char *fgets(char *s, int n, FILE *stream);
int fscanf(FILE *stream, const char *fmt, ...);
int fputc(int c, FILE *stream);
int fprintf(FILE *stream, const char *fmt, ...);
int fputs(const char *s, FILE *stream);
int ungetc(int c, FILE *stream);
int sprintf(char *buf, const char *fmt, ...);
int printf(const char *fmt, ...);
int puts(const char *str);
int putchar(int c);
int getc(FILE *stream);
int getchar(void);
int scanf(const char *fmt, ...);
int sscanf(const char *s, const char *fmt, ...);
int vfprintf(FILE *stream, const char *fmt, va_list args);
int vprintf(const char *fmt, va_list args);
int vfscanf(FILE *stream, const char *fmt, va_list args);
int vsprintf(char *s, const char *fmt, va_list args);
int vsnprintf(char *s, size_t n, const char *fmt, va_list args);
int vsscanf(const char *s, const char *fmt, va_list args);
void perror(const char *s);
int remove(const char *filename);
int rename(const char *src, const char *dst);
FILE *tmpfile(void);
char *tmpnam(char *s);

#endif
