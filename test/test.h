#ifndef HEADER_TEST_H
#define HEADER_TEST_H

#define ASSERT(expr1, expr2)    assert(__FILE__, __LINE__, (expr1), (expr2), #expr2)
void assert(char *file, int linenr, int expected, int actual, char *code);
int *allocInt4();
void free(void *p);

#endif
