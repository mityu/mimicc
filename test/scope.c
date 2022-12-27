#include "test.h"

void test_find_var_in_outer_scope(void) {
    int n;
    n = 5;
    ASSERT(5, n);
    {
        n = 10;
    }
    ASSERT(10, n);

    {
        int a;
        a = 20;
        ASSERT(20, a);
        {
            int b;
            b = a + 10;
            n = b;
            ASSERT(30, b);
        }
    }
    ASSERT(30, n);
}

void test_inner_scope_independent(void) {
    int n;
    n = 5;
    ASSERT(5, n);
    {
        int a;
        a = 10;
        ASSERT(10, a);
        ASSERT(5, n);
    }
    ASSERT(5, n);
}

int main(void) {
    test_find_var_in_outer_scope();
    test_inner_scope_independent();
    return 0;
}
