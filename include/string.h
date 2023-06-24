#ifndef __MIMICC_STRING_H
#define __MIMICC_STRING_H

#include <stddef.h>

int memcmp(const void *s1, const void *s2, size_t size);
void *memcpy(void *s1, const void *s2, size_t size);
void *memset(void *s, int c, size_t size);
char *strchr(const char *s, int c);
char *strstr(const char *s1, const char *s2);
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t len);
int strtol(const char *s, const char **end, int radix);
char *strerror(int errnum);

#endif
