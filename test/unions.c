#include "test.h"

union GUnion {};

void test_decl_global_union_var(void) {
    union GUnion obj;
}

void test_decl_local_union_var(void) {
    union LUnion {
        int n;
        int array[1][2][3], *p, **pp;
    };
    union LUnion obj;
}

void test_sizeof_union(void) {
    union S0 {};
    union S1 {
        int n;
    };
    union S2 {
        char c;
        int n; 
        void *p;
    };
    union S3 {
        char c;
        union S2 s2;
    };
    ASSERT(0, sizeof(union S0));
    ASSERT(4, sizeof(union S1));
    ASSERT(8, sizeof(union S2));
    ASSERT(8, sizeof(union S3));
}

void test_union_member_access(void) {
    union S1 {
        int n;
        int m;
        char c;
        int o;
    };
    union S2 {
        int n;
        union S1 s1;
        int m;
    };
    union S3 {
        int n;
        union S2 *ps2;
        int m;
    };

    union S1 s1;
    union S2 s2;
    union S3 s3;

    s1.n = 7;
    ASSERT(7, s1.n);
    ASSERT(7, s1.m);

    s2.s1.n = 13;
    ASSERT(13, s2.s1.n);

    s3.ps2 = &s2;
    ASSERT(13, s3.ps2->n);
}

void test_union_member_position(void) {
    union U {
        int n;
        char c;
        struct { int x[5]; } s;
    };
    union U u;

    ASSERT(1, (void *)&u.n == (void *)&u.c);
    ASSERT(1, (void *)&u.n == (void *)&u.s);
}

void test_decl_local_union_without_tag(void) {
    union U {
        int a[5];
        char c;
    } obj = {{ 3, 5, 7, 11, 13 }};

    ASSERT(3, obj.a[0]);
    ASSERT(5, obj.a[1]);
    ASSERT(7, obj.a[2]);
    ASSERT(11, obj.a[3]);
    ASSERT(13, obj.a[4]);
}

void test_union_assign(void) {
    typedef union U {
        int a[2];
        char c;
    } U;

    U u1 = {{ 3, 5 }};
    U u2 = {{ 7, 11 }};
    U u3;

    u3.c = 'x';
    u3 = u2 = u1;
    ASSERT(3, u1.a[0]);
    ASSERT(5, u1.a[1]);
    ASSERT(3, u2.a[0]);
    ASSERT(5, u2.a[1]);
    ASSERT(3, u3.a[0]);
    ASSERT(5, u3.a[1]);
}

void test_compound_literal(void) {
    typedef union U {
        int a[2];
    } U;
    {
        U u;
        u = (U){ {13, 17} };
        ASSERT(13, u.a[0]);
        ASSERT(17, u.a[1]);
    }
    {
        U u = (U){ {23, 29} };
        ASSERT(23, u.a[0]);
        ASSERT(29, u.a[1]);
    }
    {
        int x = (U){ {31, 37} }.a[1];
        ASSERT(37, x);
    }
    {
        U u1 = {{47, 49}}, u2 = {{41, 43}};
        u1 = (U){{53, 59}} = u2;
        ASSERT(41, u1.a[0]);
        ASSERT(43, u1.a[1]);
    }
    {
        int x = (&(U){ {31, 37} })->a[1];
        ASSERT(37, x);
    }
}

void test_compare_union_pointers(void) {
    union U {} x;
    ASSERT(1, &x == &x);
}

void test_union_assign_to_member_from_ptr(void) {
    union A {
        int n, m;
    };

    union B {
        int x;
        union A a;
    };

    union A a = { 13 };
    union B b = { 17 };
    union A *ap = &a;

    b.a = *ap;
    ASSERT(13, b.a.n);
}

void test_union_anonymous_member_access(void) {
    union A {
        int n;
        union {
            char text[20];
            int x;
        };
    };

    union A a = {};
    ASSERT(0, a.n);
    ASSERT(0, a.x);
    ASSERT(0, strcmp(a.text, ""));

    a.x = 5;
    ASSERT(0, a.x);
}

void test_anonymous_struct_init(void) {
    union A {
        union {
            char text[20];
            int x;
        };
        int n;
    };
    union A a1 = { {"xyzxyz"} };
    union A a2 = { "abcabc" };

    ASSERT(0, strcmp(a1.text, "xyzxyz"));
    ASSERT(0, strcmp(a2.text, "abcabc"));
}

int main(void) {
    test_decl_global_union_var();
    test_decl_local_union_var();
    test_sizeof_union();
    test_union_member_access();
    test_union_member_position();
    test_decl_local_union_without_tag();
    test_union_assign();
    test_compound_literal();
    test_compare_union_pointers();
    test_union_assign_to_member_from_ptr();
    test_union_anonymous_member_access();
}
