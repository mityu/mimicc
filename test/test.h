#ifndef HEADER_TEST_H
#define HEADER_TEST_H

#define ASSERT(expr1, expr2)    assert(__FILE__, __LINE__, (expr1), (expr2), #expr2)
// #define UNREACHABLE()   do {printf("%s:%d: unreachable", __FILE__, __LINE__); ASSERT(0, 1);} while(0)
#define UNREACHABLE()   ASSERT(0, 1)

void *safeAlloc(int size);
void assert(char *file, int linenr, int expected, int actual, char *code);
int *allocInt4();
void free(void *p);
int printf(const char *fmt, ...);

#endif
