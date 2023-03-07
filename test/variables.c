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

void test_init_local_variables(void) {
    {
        int a = 5;
        ASSERT(5, a);
    }
    {
        int a = 5, b = 10;
        int c = a + b;
        ASSERT(5, a);
        ASSERT(10, b);
        ASSERT(15, c);
    }
}

void test_init_local_arrays(void) {
    {
        int x[3] = {1, 2, 3};
        ASSERT(1, x[0]);
        ASSERT(2, x[1]);
        ASSERT(3, x[2]);
    }
    {  // Allow "," after the last element.
        int x[3] = {1, 2, 3,};
        ASSERT(1, x[0]);
        ASSERT(2, x[1]);
        ASSERT(3, x[2]);
    }
    {
        char x[3] = {'a', 'b', 'c'};
        ASSERT('a', x[0]);
        ASSERT('b', x[1]);
        ASSERT('c', x[2]);
    }
    {
        int x[2][3] = {
            {1, 2, 3},
            {4, 5, 6},
        };
        int i, j;
        for (i = 0; i < 2; ++i)
            for (j = 0; j < 3; ++j)
                ASSERT(i*3 + j + 1, x[i][j]);
    }
    {
        char x[4] = "baz";
        ASSERT('b', x[0]);
        ASSERT('a', x[1]);
        ASSERT('z', x[2]);
        ASSERT('\0', x[3]);
    }
    {
        char x[2][4] = {
            "d\nf",
            {'g', 'h', 'i', '\0'},
        };
        ASSERT('d', x[0][0]);
        ASSERT('\n', x[0][1]);
        ASSERT('f', x[0][2]);
        ASSERT('\0', x[0][3]);
        ASSERT('g', x[1][0]);
        ASSERT('h', x[1][1]);
        ASSERT('i', x[1][2]);
        ASSERT('\0', x[1][3]);
    }
    {
        int x[] = {2, 3, 4};
        ASSERT(3, sizeof(x)/sizeof(x[0]));
        ASSERT(2, x[0]);
        ASSERT(3, x[1]);
        ASSERT(4, x[2]);
    }
    {
        int x[][3] = {{1, 2, 3}, {4, 5, 6}};
        ASSERT(2, sizeof(x)/sizeof(x[0]));
        ASSERT(1, x[0][0]);
        ASSERT(2, x[0][1]);
        ASSERT(3, x[0][2]);
        ASSERT(4, x[1][0]);
        ASSERT(5, x[1][1]);
        ASSERT(6, x[1][2]);
    }
    {
        char x[] = "abcd\n";
        ASSERT(6, sizeof(x)/sizeof(x[0]));
        ASSERT('a', x[0]);
        ASSERT('b', x[1]);
        ASSERT('c', x[2]);
        ASSERT('d', x[3]);
        ASSERT('\n', x[4]);
        ASSERT('\0', x[5]);
    }
    {
        int n[3] = {3, 5, 7}, m = 11;
        ASSERT(3, n[0]);
        ASSERT(5, n[1]);
        ASSERT(7, n[2]);
        ASSERT(11, m);
    }
}

void test_init_local_structs(void) {
    struct S1 {
        int n;
        int m;
    };
    struct S2 {
        char c;
        int n;
    };
    struct S3 {
        struct S2 s2;
        char c;
    };
    struct S4 {
        int n[4];
    };

    {
        struct S1 obj = {3, 7};
        ASSERT(3, obj.n);
        ASSERT(7, obj.m);
    }
    {
        struct S2 obj = {'a', 7};
        ASSERT(97, obj.c);
        ASSERT(7, obj.n);
    }
    {
        struct S3 obj = {{'l', 11}, 'z'};
        ASSERT(108, obj.s2.c);
        ASSERT(11, obj.s2.n);
        ASSERT(122, obj.c);
    }
    {
        struct S4 obj = {{13, 17, 19, 23}};
        ASSERT(13, obj.n[0]);
        ASSERT(17, obj.n[1]);
        ASSERT(19, obj.n[2]);
        ASSERT(23, obj.n[3]);
    }
    {
        struct S1 obj[3] = {{29, 31}, {37, 41}, {43, 47}};
        ASSERT(29, obj[0].n);
        ASSERT(31, obj[0].m);
        ASSERT(37, obj[1].n);
        ASSERT(41, obj[1].m);
        ASSERT(43, obj[2].n);
        ASSERT(47, obj[2].m);
    }
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

void test_zero_clear_local_array(void) {
    {
        int n[3][3] = {{}, {101, 103},};
        ASSERT(0, n[0][0]);
        ASSERT(0, n[0][1]);
        ASSERT(0, n[0][2]);
        ASSERT(101, n[1][0]);
        ASSERT(103, n[1][1]);
        ASSERT(0, n[1][2]);
        ASSERT(0, n[2][0]);
        ASSERT(0, n[2][1]);
        ASSERT(0, n[2][2]);
    }
    {
        char s[5] = "xyz";
        ASSERT(120, s[0]);
        ASSERT(121, s[1]);
        ASSERT(122, s[2]);
        ASSERT(0, s[3]);
        ASSERT(0, s[4]);
    }
}

void test_zero_clear_local_struct(void) {
    struct S0 {
        int n;
        int narray1[3];
        int narray3[3][3][3];
    };
    struct S {
        int n;
        struct S0 s0;
        struct S0 s0array[3];
    };
    {
        struct S0 obj = {};
        ASSERT(0, obj.n);
        ASSERT(0, obj.narray1[0]);
        ASSERT(0, obj.narray1[1]);
        ASSERT(0, obj.narray1[2]);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k)
                    ASSERT(0, obj.narray3[i][j][k]);
    }
    {
        struct S0 obj = {3, {5, 7, 11}, {}};
        ASSERT(3, obj.n);
        ASSERT(5, obj.narray1[0]);
        ASSERT(7, obj.narray1[1]);
        ASSERT(11, obj.narray1[2]);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k)
                    ASSERT(0, obj.narray3[i][j][k]);
    }
    {
        struct S obj = {13, {}, {{}, {17, {19, 23, 29}, {}},}};
        ASSERT(13, obj.n);

        ASSERT(0, obj.s0.n);
        ASSERT(0, obj.s0.narray1[0]);
        ASSERT(0, obj.s0.narray1[1]);
        ASSERT(0, obj.s0.narray1[2]);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k)
                    ASSERT(0, obj.s0.narray3[i][j][k]);

        ASSERT(0, obj.s0array[0].n);
        ASSERT(0, obj.s0array[0].narray1[0]);
        ASSERT(0, obj.s0array[0].narray1[1]);
        ASSERT(0, obj.s0array[0].narray1[2]);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k)
                    ASSERT(0, obj.s0array[0].narray3[i][j][k]);

        ASSERT(17, obj.s0array[1].n);
        ASSERT(19, obj.s0array[1].narray1[0]);
        ASSERT(23, obj.s0array[1].narray1[1]);
        ASSERT(29, obj.s0array[1].narray1[2]);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k)
                    ASSERT(0, obj.s0array[1].narray3[i][j][k]);

        ASSERT(0, obj.s0array[2].n);
        ASSERT(0, obj.s0array[2].narray1[0]);
        ASSERT(0, obj.s0array[2].narray1[1]);
        ASSERT(0, obj.s0array[2].narray1[2]);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k)
                    ASSERT(0, obj.s0array[2].narray3[i][j][k]);
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
    test_init_local_variables();
    test_init_local_arrays();
    test_zero_clear_local_array();
    test_zero_clear_local_struct();
    test_local_variables();
    test_increment_or_decrement_array_element();
    test_global_variables();
    ASSERT(10, local_var_independent());
    test_static_local_variable();
    test_access_results_in_array();
    return 0;
}
