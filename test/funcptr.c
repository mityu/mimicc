#include "test.h"

int counter;
int another_counter;

// Test only compiler accepts function pointer declaration.
void (*g_fptr)(void);
void (*(*g_fptr_ret_fptr)(void))(void);
void (*g_fptr_accept_fptr)(void (*)(void));

void inc_counter(void) {
    counter++;
}

int inc_another_counter(void) {
    return ++another_counter;
}

void call_fptr_and_inc_counter(int n) {
    (void)n;
    counter++;
}

void (*ret_inc_counter(void))(void) {
    return inc_counter;
}

void (*(*ret_ret_inc_counter(void))(void))(void) {
    return ret_inc_counter;
}

void call_fptr(void (*fptr)(void)) {
    fptr();
}

int add3(int n1, int n2, int n3) {
    return n1 + n2 + n3;
}

int add7(int a, int b, int c, int d, int e, int f, int g) {
    return a + b + c + d + e + f + g;
}

void testFptrInGlobalScope(void) {
    counter = 0;
    ASSERT(0, counter);

    g_fptr = inc_counter;
    g_fptr_ret_fptr = ret_inc_counter;
    g_fptr_accept_fptr = call_fptr;

    g_fptr();
    ASSERT(1, counter);

    g_fptr_ret_fptr()();
    ASSERT(2, counter);
    counter = 2;

    g_fptr_accept_fptr(inc_counter);
    ASSERT(3, counter);

    g_fptr_accept_fptr(g_fptr);
    ASSERT(4, counter);
}

void testFptrInLocalScope(void) {
    void (*fptr)(void);
    void (*(*fptr_ret_fptr)(void))(void);
    void (*fptr_accept_fptr)(void (*)(void));
    void (*(*(*fptr_ret_fptr_ret_fptr)(void))(void))(void);
    void (*fptr2)(void) = inc_counter;

    counter = 0;
    ASSERT(0, counter);

    fptr = inc_counter;
    fptr_ret_fptr = ret_inc_counter;
    fptr_accept_fptr = call_fptr;
    fptr_ret_fptr_ret_fptr = ret_ret_inc_counter;

    fptr();
    ASSERT(1, counter);

    fptr_ret_fptr()();
    ASSERT(2, counter);
    counter = 2;

    fptr_accept_fptr(inc_counter);
    ASSERT(3, counter);

    fptr_accept_fptr(fptr);
    ASSERT(4, counter);

    fptr2();
    ASSERT(5, counter);

    fptr_ret_fptr_ret_fptr()()();
    ASSERT(6, counter);
}

void testFptrInStruct(void) {
    struct S {
        void (*fptr)(void);
        void (*(*fptr_ret_fptr)(void))(void);
        void (*fptr_accept_fptr)(void (*)(void));
    };

    struct S s;

    counter = 0;
    ASSERT(0, counter);

    s.fptr = inc_counter;
    s.fptr_ret_fptr = ret_inc_counter;
    s.fptr_accept_fptr = call_fptr;

    s.fptr();
    ASSERT(1, counter);

    s.fptr_ret_fptr()();
    ASSERT(2, counter);
    counter = 2;

    s.fptr_accept_fptr(inc_counter);
    ASSERT(3, counter);

    s.fptr_accept_fptr(s.fptr);
    ASSERT(4, counter);
}

void testFptrWithMultipleArgs(void) {
    int (*p_add3)(int n1, int n2, int n3) = add3;
    int (*p_add7)(int a, int b, int c, int d, int e, int f, int g) = add7;
    int n;

    n = 0;
    ASSERT(0, n);

    n = p_add3(11, 13, 17);
    ASSERT(41, n);
    ASSERT(10, p_add3(2, 3, 5));
    ASSERT(127, p_add3(p_add3(3, 5, 7), p_add3(11, 13, 17), p_add3(19, 23, 29)));

    n = p_add7(2, 3, 4, 5, 6, 7, 8);
    ASSERT(35, n);
    ASSERT(42, p_add7(3, 4, 5, 6, 7, 8, 9));
    ASSERT(104, p_add7(p_add7(2, 3, 4, 5, 6, 7, 8), p_add7(1, 1, 1, 1, 1, 1, 3), 10, 11, 12, 13, 14));
}

void testFptrCallAtArgument(void) {
    void (*outer)(int) = call_fptr_and_inc_counter;
    int (*inner)(void) = inc_another_counter;

    counter = 0;
    another_counter = 0;
    outer(inner());
    ASSERT(1, counter);
    ASSERT(1, another_counter);
}

void testFptrAddress(void) {
    void (*fp)(void);
    void (*fp2)(void) = &inc_counter;
    void (**fpp)(void);
    fp = &inc_counter;
    fpp = &fp;

    counter = 0;
    fp();
    ASSERT(1, counter);
    fp2();
    ASSERT(2, counter);
    (*fpp)();
    ASSERT(3, counter);
}

void testFptrDereference(void) {
    void (*fp)(void);
    void (**fpp)(void);
    fp = &inc_counter;
    fpp = &fp;

    counter = 0;
    (*fp)();
    ASSERT(1, counter);
    (***fp)();
    ASSERT(2, counter);
    (****inc_counter)();
    ASSERT(3, counter);
    (*fpp)();
    ASSERT(4, counter);
    (****fpp)();
    ASSERT(5, counter);
}

int main(void) {
    testFptrInGlobalScope();
    testFptrInLocalScope();
    testFptrInStruct();
    testFptrWithMultipleArgs();
    testFptrCallAtArgument();
    testFptrAddress();
    testFptrDereference();
    return 0;
}
