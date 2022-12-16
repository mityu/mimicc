#include "test.h"

// Recursive function test
int fib(int n) {
    if (n <= 1)
        return 1;
    else
        return fib(n - 1) + fib(n - 2);
}

// assert 97 'void *malloc(int); int sprintf(char *, char *, ...); int main(void) {}'
// Function call with variadic arguments test
int sprintf(char *, char *, ...);
void testFuncCallWithVaArgs(void) {
    char *s;
    s = safeAlloc(4 * sizeof(char));
    sprintf(s, "%s", "abc");
    ASSERT('a', *s);
    ASSERT('b', s[1]);
    ASSERT('c', s[2]);
    ASSERT('\0', s[3]);
    free(s);
}

// Function declaration test
int add(int, int);
int add(int a, int b) {
    return a + b;
}

int funcRefArgs(int a, int b) {
    int n1, n2;
    n1 = a;
    n2 = b;
    return n1*3 + n2*2;
}

void funcPtrArg(int *n) {
    *n = 12345;
}
void testFunPtrArg() {
    int n;
    n = 25;
    ASSERT(25, n);
    funcPtrArg(&n);
    ASSERT(12345, n);
}

// Test function call with over 6 arguments.
int funcArg7(int n1, int n2, int n3, int n4, int n5, int n6, int n7) {
    int a;
    a = 25;
    return n7 + a;
}
void testFuncArg7() {
    funcArg7(1, 2, 3, 4, 5, 6, 7);
    ASSERT(32, funcArg7(1, 2, 3, 4, 5, 6, 7));
}

int funcArg8(int n1, int n2, int n3, int n4, int n5, int n6, int n7, int n8) {
    return n8;
}
void testFuncArg8() {
    ASSERT(8, funcArg8(1, 2, 3, 4, 5, 6, 7, 8));
    ASSERT(42, funcArg8(1, 2, 3, 4, 5, 6, funcArg8(1, 2, 3, 4, 5, 6, 7, 8), 42));
}


int main(void) {
    ASSERT(10, add(3, 7));
    ASSERT(13, fib(6));
    ASSERT(23, funcRefArgs(5, 4));
    testFuncArg7();
    testFuncArg8();
    return 0;
}
