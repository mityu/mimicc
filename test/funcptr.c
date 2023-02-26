#include "test.h"

// Test only compiler accepts function pointer declaration.
void (*g_fptr)(void);
void (*(*g_fptr_ret_fptr)(void))(void);
void (*g_fptr_accept_fptr)(void (*)(void));

void testFptrInStruct(void) {
    struct S {
        void (*fptr)(void);
        void (*(*fptr_ret_fptr)(void))(void);
        void (*fptr_accept_fptr)(void (*)(void));
    };
}



int main(void) {
    testFptrInStruct();
    return 0;
}
