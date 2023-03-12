#include "test.h"

struct GStruct {};
struct GStruct gObj;

struct {int n[2]; char s[3];} gObjWithoutTag;

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

void test_decl_local_struct_without_tag(void) {
    struct S {
        int n;
        char c;
    } obj = {3, 'a'}, zero = {};

    ASSERT(3, obj.n);
    ASSERT(97, obj.c);
    ASSERT(0, zero.n);
    ASSERT(0, zero.c);
}

void test_decl_global_struct_without_tag(void) {
    ASSERT(0, gObjWithoutTag.n[0]);
    ASSERT(0, gObjWithoutTag.n[1]);
    ASSERT(0, gObjWithoutTag.s[0]);
    ASSERT(0, gObjWithoutTag.s[1]);
    ASSERT(0, gObjWithoutTag.s[2]);
}

void test_struct_assign(void) {
    typedef struct S {
        int a[5];
        char c;
        int n;
    } S;
    S s1 = {{11, 13, 17, 19, 23}, 'a', 29};
    S s2 = {}, s3 = {};

    s3 = s2 = s1;
    ASSERT(11, s2.a[0]);
    ASSERT(13, s2.a[1]);
    ASSERT(17, s2.a[2]);
    ASSERT(19, s2.a[3]);
    ASSERT(23, s2.a[4]);
    ASSERT(97, s2.c);
    ASSERT(29, s2.n);
    ASSERT(11, s3.a[0]);
    ASSERT(13, s3.a[1]);
    ASSERT(17, s3.a[2]);
    ASSERT(19, s3.a[3]);
    ASSERT(23, s3.a[4]);
    ASSERT(97, s3.c);
    ASSERT(29, s3.n);
    ASSERT(11, s1.a[0]);
    ASSERT(13, s1.a[1]);
    ASSERT(17, s1.a[2]);
    ASSERT(19, s1.a[3]);
    ASSERT(23, s1.a[4]);
    ASSERT(97, s1.c);
    ASSERT(29, s1.n);

    s1.a[2] = 31;
    ASSERT(31, s1.a[2]);
    ASSERT(17, s2.a[2]);
}

void test_compound_literal(void) {
    typedef struct S {
        int n;
        int m;
    } S;
    {
        S s;
        s = (S){13, 19};
        ASSERT(13, s.n);
        ASSERT(19, s.m);
    }
    {
        S s = (S){23, 29};
        ASSERT(23, s.n);
        ASSERT(29, s.m);
    }
    {
        int m = (S){31, 37}.m;
        ASSERT(37, m);
    }
    {
        S s1 = {}, s2 = {41, 43};
        s1 = (S){47, 53} = s2;
        ASSERT(41, s1.n);
        ASSERT(43, s1.m);
    }
    {
        int m = (&(S){57, 59})->m;
        ASSERT(57, m);
    }
}

void test_compare_struct_pointers(void) {
    struct S {} a;
    ASSERT(1, &a == &a);
}

int main(void) {
    test_decl_global_struct_var();
    test_decl_local_struct_var();
    test_sizeof_struct();
    test_struct_member_access();
    test_decl_local_struct_without_tag();
    test_decl_global_struct_without_tag();
    test_struct_assign();
    test_compare_struct_pointers();
    return 0;
}
