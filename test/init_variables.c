#include "test.h"

int strcmp(const char *, const char *);

typedef struct {
    int n;
    char c;
} GS0;
typedef struct {
    int ar[5];
    char c;
    char str[10];
    GS0 s0;
} GS1;

int gnum = 101;
char gchar = 'u';
int gnumar1[5] = {17, 19, 23};
int gnumar2[] = {29, 31, 37};
int gnumarar1[3][3] = {{41, 47, 53}};
int gnumarar2[][3] = {{59, 61, 67}};
char gstr1[10] = "abc";
char gstr2[] = "xyz";
char gstr3[] = {'d', 'e'};
char gstrar[2][5] = {"jjj", "kkk"};
GS0 gs0 = {69, 71};
GS0 gs0ar[2] = {{73, 79}};
GS1 gs1 = {{83, 87}, 'w', "goodbye", {89, 97}};
GS1 gs1rest = {{103, 107}, 'g', "space"};


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

void test_init_global_primitives(void) {
    ASSERT(101, gnum);
    ASSERT(117, gchar);
}

void test_init_global_arrays(void) {
    ASSERT(5 * sizeof(int), sizeof(gnumar1));
    ASSERT(17, gnumar1[0]);
    ASSERT(19, gnumar1[1]);
    ASSERT(23, gnumar1[2]);
    ASSERT(0, gnumar1[3]);
    ASSERT(0, gnumar1[4]);

    ASSERT(3 * sizeof(int), sizeof(gnumar2));
    ASSERT(29, gnumar2[0]);
    ASSERT(31, gnumar2[1]);
    ASSERT(37, gnumar2[2]);

    ASSERT(9 * sizeof(int), sizeof(gnumarar1));
    ASSERT(41, gnumarar1[0][0]);
    ASSERT(47, gnumarar1[0][1]);
    ASSERT(53, gnumarar1[0][2]);
    for (int i = 0; i < 3; ++i) {
        ASSERT(0, gnumarar1[1][i]);
        ASSERT(0, gnumarar1[2][i]);
    }

    ASSERT(3 * sizeof(int), sizeof(gnumarar2));
    ASSERT(59, gnumarar2[0][0]);
    ASSERT(61, gnumarar2[0][1]);
    ASSERT(67, gnumarar2[0][2]);

    ASSERT(10, sizeof(gstr1));
    ASSERT(0, strcmp(gstr1, "abc"));
    ASSERT(0, gstr1[8]);

    ASSERT(4, sizeof(gstr2));
    ASSERT(0, strcmp(gstr2, "xyz"));

    ASSERT(2, sizeof(gstr3));
    ASSERT(100, gstr3[0]);
    ASSERT(101, gstr3[1]);

    ASSERT(10, sizeof(gstrar));
    ASSERT(0, strcmp(gstrar[0], "jjj"));
    ASSERT(0, strcmp(gstrar[1], "kkk"));
}

void test_init_global_structs(void) {
    ASSERT(69, gs0.n);
    ASSERT(71, gs0.c);

    ASSERT(2 * sizeof(GS0), sizeof(gs0ar));
    ASSERT(73, gs0ar[0].n);
    ASSERT(79, gs0ar[0].c);
    ASSERT(0, gs0ar[1].n);
    ASSERT(0, gs0ar[1].c);

    ASSERT(83, gs1.ar[0]);
    ASSERT(87, gs1.ar[1]);
    ASSERT(0, gs1.ar[3]);
    ASSERT(119, gs1.c);
    ASSERT(0, strcmp(gs1.str, "goodbye"));
    ASSERT(0, gs1.str[9]);
    ASSERT(89, gs1.s0.n);
    ASSERT(97, gs1.s0.c);

    ASSERT(103, gs1rest.ar[0]);
    ASSERT(107, gs1rest.ar[1]);
    ASSERT(0, gs1rest.ar[3]);
    ASSERT(103, gs1rest.c);
    ASSERT(0, strcmp(gs1rest.str, "space"));
    ASSERT(0, gs1rest.str[8]);
    ASSERT(0, gs1rest.s0.n);
    ASSERT(0, gs1rest.s0.c);
}

void test_init_static_primitives(void) {
    static int zero;
    static int n = 13;
    ASSERT(0, zero);
    ASSERT(13, n);
}

void test_init_static_arrays(void) {
    static int nums1[10] = {3, 5, 7, 11, 13};
    static int nums2[] = {23, 29, 31, 37};
    static char s1[10] = "abc";
    static char s2[] = "xyz";
    static char s3[] = {'i', 'o', 's'};
    static char ss1[4][10] = {"fgh", "jkl", "opq"};
    static char ss2[][10] = {"yuio", "hjkl", "nm,."};

    ASSERT(10 * sizeof(int), sizeof(nums1));
    ASSERT(3, nums1[0]);
    ASSERT(5, nums1[1]);
    ASSERT(7, nums1[2]);
    ASSERT(11, nums1[3]);
    ASSERT(13, nums1[4]);
    ASSERT(0, nums1[5]);
    ASSERT(0, nums1[6]);
    ASSERT(0, nums1[7]);
    ASSERT(0, nums1[8]);
    ASSERT(0, nums1[9]);

    ASSERT(4 * sizeof(int), sizeof(nums2));
    ASSERT(23, nums2[0]);
    ASSERT(29, nums2[1]);
    ASSERT(31, nums2[2]);
    ASSERT(37, nums2[3]);

    ASSERT(10, sizeof(s1));
    ASSERT(0, strcmp(s1, "abc"));
    ASSERT(0, s1[5]);

    ASSERT(4, sizeof(s2));
    ASSERT(0, strcmp(s2, "xyz"));

    ASSERT(3, sizeof(s3));
    ASSERT(105, s3[0]);
    ASSERT(111, s3[1]);
    ASSERT(115, s3[2]);

    ASSERT(40, sizeof(ss1));
    ASSERT(0, strcmp(ss1[0], "fgh"));
    ASSERT(0, strcmp(ss1[1], "jkl"));
    ASSERT(0, strcmp(ss1[2], "opq"));
    ASSERT(0, strcmp(ss1[3], ""));
    ASSERT(0, ss1[0][5]);

    ASSERT(30, sizeof(ss2));
    ASSERT(0, strcmp(ss2[0], "yuio"));
    ASSERT(0, strcmp(ss2[1], "hjkl"));
    ASSERT(0, strcmp(ss2[2], "nm,."));
    ASSERT(0, ss2[0][5]);
}

void test_init_static_structs(void) {
    typedef struct {
        int n;
        char c;
    } S0;
    typedef struct {
        int ar[5];
        char c;
        char str[10];
        S0 s0;
    } S1;

    static S0 s0 = {7, 11};
    static S0 s0ar[2] = {{13, 17}};
    static S1 s1 = {{19, 23}, 'x', "hello", {29, 31}};
    static S1 s1rest = {{37, 41}, 'h', "world"};

    ASSERT(7, s0.n);
    ASSERT(11, s0.c);

    ASSERT(2 * sizeof(S0), sizeof(s0ar));
    ASSERT(13, s0ar[0].n);
    ASSERT(17, s0ar[0].c);
    ASSERT(0, s0ar[1].n);
    ASSERT(0, s0ar[1].c);

    ASSERT(19, s1.ar[0]);
    ASSERT(23, s1.ar[1]);
    ASSERT(0, s1.ar[3]);
    ASSERT(120, s1.c);
    ASSERT(0, strcmp(s1.str, "hello"));
    ASSERT(0, s1.str[8]);
    ASSERT(29, s1.s0.n);
    ASSERT(31, s1.s0.c);

    ASSERT(37, s1rest.ar[0]);
    ASSERT(41, s1rest.ar[1]);
    ASSERT(0, s1rest.ar[3]);
    ASSERT(104, s1rest.c);
    ASSERT(0, strcmp(s1rest.str, "world"));
    ASSERT(0, s1rest.str[8]);
    ASSERT(0, s1rest.s0.n);
    ASSERT(0, s1rest.s0.c);
}

int main(void) {
    test_init_local_variables();
    test_init_local_arrays();
    test_zero_clear_local_array();
    test_zero_clear_local_struct();
    test_init_global_primitives();
    test_init_global_arrays();
    test_init_global_structs();
    test_init_static_primitives();
    test_init_static_arrays();
    test_init_static_structs();
}
