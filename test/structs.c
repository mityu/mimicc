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

int main(void) {
    test_decl_global_struct_var();
    test_decl_local_struct_var();
    test_sizeof_struct();
    return 0;
}
