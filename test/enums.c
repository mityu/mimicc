#include "test.h"

void testLocalEnumWithTag(void) {
    enum A {
        ItemA,
        ItemB,
        ItemC,
    };

    ASSERT(0, ItemA);
    ASSERT(1, ItemB);
    ASSERT(2, ItemC);
}

int main(void) {
    testLocalEnumWithTag();
    return 0;
}
