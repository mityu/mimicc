#include "test.h"

void testBasicTypeCastWithPrimitiveType(void) {
    {
        int n = 19 + 256 + 512;
        ASSERT(787, n);
        n = (int)(char)n;
        ASSERT(19, n);
    }
    {
        char c = 'a';
        int n = (int)c;
        ASSERT(97, n);
    }
}

void testBasicTypeCastWithPointerType(void) {
    int n = 13;
    void *pVoid = (void *)&n;
    int *pInt = (int *)pVoid;

    ASSERT(13, n);
    ASSERT(1, pInt == &n);
    ASSERT(13, *pInt);
    ASSERT(13, *(int *)pVoid);

    *pInt = 17;
    ASSERT(17, n);

    *(int *)pVoid = 23;
    ASSERT(23, n);
}

int main(void) {
    testBasicTypeCastWithPrimitiveType();
    testBasicTypeCastWithPointerType();
    return 0;
}
