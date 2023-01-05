#include "test.h"

struct GStruct {};
struct GStruct gObj;

void test_decl_global_struct_var(void) {
    struct GStruct obj;
}

void test_decl_local_struct_var(void) {
    struct LStruct {};
    struct LStruct obj;
}

int main(void) {
    test_decl_global_struct_var();
    test_decl_local_struct_var();
    return 0;
}
