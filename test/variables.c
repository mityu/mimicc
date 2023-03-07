#include "test.h"

int g_num1;
int *g_ptr, g_array[3], g_marray1[2][3], g_marray2[2][3][4], g_num2;

void test_global_variables(void) {
    int i, j;

    g_num1 = 10;
    g_array[0] = 100;
    g_array[1] = 200;
    g_array[2] = 300;
    g_marray2[1][2][2] = 100;
    g_marray2[1][2][3] = 200;
    g_num2 = 400;

    ASSERT(10, g_num1);
    ASSERT(100, g_array[0]);
    ASSERT(200, g_array[1]);
    ASSERT(300, g_array[2]);
    ASSERT(400, g_num2);
    ASSERT(100, g_marray2[1][2][2]);
    ASSERT(200, g_marray2[1][2][3]);

    for (i = 0; i < 2; ++i)
        for (j = 0; j < 3; ++j)
            g_marray1[i][j] = 3 * i + j + 1;
    for (i = 0; i < 2; ++i)
        for (j = 0; j < 3; ++j)
            ASSERT(3 * i + j + 1, g_marray1[i][j]);

    g_ptr = &g_num2;
    *g_ptr = 42;
    ASSERT(42, *g_ptr);
    ASSERT(42, g_num2);

    g_ptr = g_array;
    ASSERT(100, *g_ptr);
    ASSERT(100, g_ptr[0]);
    ASSERT(200, g_ptr[1]);
    ASSERT(300, g_ptr[2]);

    *g_ptr = 1000;
    ASSERT(1000, *g_ptr);
    ASSERT(1000, g_array[0]);

    g_ptr++;
    ASSERT(200, *g_ptr);

    g_ptr--;
    ASSERT(1000, *g_ptr);

    g_ptr += 2;
    ASSERT(300, *g_ptr);

    g_ptr -= 2;
    ASSERT(1000, *g_ptr);
}

void test_local_variables(void) {
    {
        int a;
        a = 10;
        ASSERT(10, a);
    }
    {
        int a;
        a = -(-5*+15);
        ASSERT(75, a);
    }
    {
        int a, b;
        a = 10;
        b = a;
        ASSERT(10, a);
        ASSERT(10, b);
    }
    {
        int a, b;
        a = b = 10;
        ASSERT(10, a);
        ASSERT(10, b);
    }
    {
        int a, b;
        a = 3;
        b = 10;
        ASSERT(13, a + b);
        ASSERT(30, a * b);
        ASSERT(1, a < b);
        ASSERT(0, a > b);
    }
    {
        int foo;
        foo = 10;
        ASSERT(10, foo);
    }
    {
        int foo, bar;
        foo = 10;
        bar = 20;
        ASSERT(10, foo);
        ASSERT(20, bar);
        ASSERT(200, foo * bar);

        bar = foo;
        ASSERT(10, foo);
        ASSERT(10, bar);

        bar = (foo * foo == 100);
        ASSERT(1, bar);

        bar = 0;
        ASSERT(0, bar);

        bar = ((foo = 5) == 5);
        ASSERT(5, foo);
        ASSERT(1, bar);
    }
    {
        int a, b, c, d;
        a = 4;
        b = 3;
        c = 2;
        d = 1;
        ASSERT(10, a + b + c + d);
    }
    {
        int a, b, c, d;
        a = 4;
        b = 3;
        c = 2;
        d = 1;
        a -= b -= c -= d;
        ASSERT(2, a);
        ASSERT(2, b);
        ASSERT(1, c);
        ASSERT(1, d);
    }
    {
        int n;
        n = 10;
        ASSERT(10, n);
        ASSERT(11, ++n);
        ASSERT(11, n);
        ASSERT(10, --n);
        ASSERT(10, n);
        ASSERT(10, n++);
        ASSERT(11, n);
        ASSERT(11, n--);
        ASSERT(10, n);

        n += 100;
        ASSERT(110, n);

        n -= 50;
        ASSERT(60, n);
    }
    {
        int a[2];
        *a = 2;
        ASSERT(2, *a);
        ASSERT(2, a[0]);

        *(a+1) = 5;
        ASSERT(5, *(a+1));
        ASSERT(5, a[1]);
        ASSERT(2, *a);
    }
    {
        int c;
        int b[2];
        int a;
        a = 10;
        b[0] = 3;
        b[1] = 5;
        c = 100;
        ASSERT(10, a);
        ASSERT(3, b[0]);
        ASSERT(5, b[1]);
        ASSERT(100, c);
    }
    {
        int *p, *save;
        p = allocInt4();
        save = p;

        ASSERT(1, p[0]);
        ASSERT(2, p[1]);
        ASSERT(3, p[2]);
        ASSERT(4, p[3]);
        ASSERT(2, *(++p));
        free(save);
    }
    {
        int n, *p;
        n = 100;
        p = &n;
        ASSERT(100, n);
        ASSERT(100, *p);
        *p = 200;
        ASSERT(200, n);
    }
    {
        int n, *p, **pp;
        n = 100;
        p = &n;
        pp = &p;
        ASSERT(100, **pp);
        **pp = 200;
        ASSERT(200, n);
    }
    {
        int x[2][3];
        int i, j;
        for (i = 0; i < 2; ++i)
            for (j = 0; j < 3; ++j)
                x[i][j] = 3 * i + j + 1;

        for (i = 0; i < 2; ++i)
            for (j = 0; j < 3; ++j)
                ASSERT(3 * i + j + 1, x[i][j]);
    }
    {
        char a, b;
        a = 3;
        b = 8;
        ASSERT(3, a);
        ASSERT(8, b);
        ASSERT(11, a + b);
        ASSERT(0, a == b);

        a++;
        ASSERT(4, a);
        a--;
        ASSERT(3, a);

        a = 'a';
        ASSERT(97, a);
    }
    {
        char x[3];
        int y;
        x[0] = 2;
        x[1] = -1;
        y = 4;
        ASSERT(3, x[1] + y);
    }
    {
        char x[3];
        char *p;
        x[0] = 1;
        x[1] = 2;
        x[2] = 3;
        p = x;
        p++;
        ASSERT(2, *p);
    }
    {
        char *s;

        s = "abc";
        ASSERT('a', *s);
        ASSERT('b', s[1]);
        ASSERT('c', s[2]);

        s = "def";
        ASSERT('d', *s);
        ASSERT('e', s[1]);
        ASSERT('f', s[2]);
    }
    {
        void *p;
        int n;
        p = &n;
        ASSERT(1, p == &n);
    }
    {
        void *vp;
        int n, *p;
        p = &n;
        vp = &p;
        ASSERT(1, vp == &p);
    }
}

void test_increment_or_decrement_array_element(void) {
    int x[3] = {3, 5, 7};
    x[2]++;
    ASSERT(8, x[2]);
    x[2]--;
    ASSERT(7, x[2]);
    ++x[2];
    ASSERT(8, x[2]);
    --x[2];
    ASSERT(7, x[2]);
}

int local_var_independent(void) {
    int a[2];
    *a = 5;
    return 10;
}

int helper_countup(void) {
    static int count;
    return ++count;
}

int helper_countup2(void) {
    static int count1;
    static int count2;
    count1++;
    count2++;
    return count1 + count2;
}

void test_static_local_variable(void) {
    ASSERT(1, helper_countup());
    ASSERT(2, helper_countup());
    ASSERT(3, helper_countup());

    ASSERT(2, helper_countup2());
    ASSERT(4, helper_countup2());
    ASSERT(6, helper_countup2());

    ASSERT(4, helper_countup());
}

void test_access_results_in_array(void) {
    int arar[][5] = {{3, 5, 7, 9, 11}, {13, 17, 19, 23, 29}};
    int *par = arar[1];

    ASSERT(13, par[0]);
    ASSERT(17, par[1]);
    ASSERT(19, par[2]);
    ASSERT(23, par[3]);
    ASSERT(29, par[4]);
}

int main(void) {
    test_local_variables();
    test_increment_or_decrement_array_element();
    test_global_variables();
    ASSERT(10, local_var_independent());
    test_static_local_variable();
    test_access_results_in_array();
    return 0;
}
