#include "test.h"

struct GStruct {};
struct GStruct gObj;

void test_decl_global_struct_var(void) {
    struct GStruct obj;
}

void test_decl_local_struct_var(void) {
    struct LStruct {
        int n;
        int array[1][2][3], *p, **pp;
    };
    struct LStruct obj;
}

void test_sizeof_struct(void) {
    struct S0 {};
    struct S1 {
        int n;
    };
    struct S2 {
        char c;
        int n;
    };
    struct S3 {
        struct S2 s2;
        char c;
    };

    ASSERT(0, sizeof(struct S0));
    ASSERT(4, sizeof(struct S1));
    ASSERT(8, sizeof(struct S2));
    ASSERT(12, sizeof(struct S3));
}

void test_struct_member_access(void) {
    struct S1 {
        int n;
        int m;
        char c;
        int o;
    };
    struct S2 {
        int n;
        struct S1 s1;
        int m;
    };
    struct S3 {
        int n;
        struct S2 *ps2;
        int m;
    };

    struct S1 s1;
    struct S2 s2;
    struct S3 s3;

    s1.n = 7;
    s1.m = 11;
    s1.c = 13;
    s1.o = 17;

    ASSERT(7, s1.n);
    ASSERT(11, s1.m);
    ASSERT(13, s1.c);
    ASSERT(17, s1.o);

    s2.n = 19;
    s2.s1.n = 23;
    s2.s1.m = 29;
    s2.s1.c = 31;
    s2.s1.o = 37;
    s2.m = 41;

    ASSERT(19, s2.n);
    ASSERT(23, s2.s1.n);
    ASSERT(29, s2.s1.m);
    ASSERT(31, s2.s1.c);
    ASSERT(37, s2.s1.o);
    ASSERT(41, s2.m);

    s3.n = 43;
    s3.m = 47;
    s3.ps2 = &s2;

    ASSERT(43, s3.n);
    ASSERT(47, s3.m);
    ASSERT(19, (*s3.ps2).n);
    ASSERT(23, (*s3.ps2).s1.n);
    ASSERT(29, (*s3.ps2).s1.m);
    ASSERT(31, (*s3.ps2).s1.c);
    ASSERT(37, (*s3.ps2).s1.o);
    ASSERT(41, (*s3.ps2).m);
    ASSERT(19, s3.ps2->n);
    ASSERT(23, s3.ps2->s1.n);
}

int main(void) {
    test_decl_global_struct_var();
    test_decl_local_struct_var();
    test_sizeof_struct();
    test_struct_member_access();
    return 0;
}
