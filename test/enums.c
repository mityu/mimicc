#include "test.h"

void testLocalEnumWithTag(void) {
    enum A {
        ItemA,
        ItemB,
        ItemC,
    };

    enum A ev = ItemA;
    int n = ItemC;

    ASSERT(0, ItemA);
    ASSERT(1, ItemB);
    ASSERT(2, ItemC);
    ASSERT(0, ev);
    ASSERT(2, n);

    ev = ItemB;
    ASSERT(1, ev);

    ev = 13;
    ASSERT(13, ev);
}

void testSizeOfEnum(void) {
    enum A {
        ItemA,
        ItemB,
    };

    ASSERT(4, sizeof(enum A));
}

int main(void) {
    testLocalEnumWithTag();
    testSizeOfEnum();
    return 0;
}
