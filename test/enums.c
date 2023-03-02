#include "test.h"

enum GA {
    GItemA,
    GItemB,
    GItemC
};

enum GAvar {
    GItemX,
    GItemY,
    GItemZ
} gNamed;

enum {
    GNamelessA,
    GNamelessB,
    GNamelessC,
} gNameless;

enum {
    GNamelessX,
    GNamelessY,
    GNamelessZ,
};

void testGlobalEnumWithTag(void) {
    ASSERT(0, GItemA);
    ASSERT(1, GItemB);
    ASSERT(2, GItemC);

    ASSERT(0, GItemX);
    ASSERT(1, GItemY);
    ASSERT(2, GItemZ);

    gNamed = GItemY;
    ASSERT(1, gNamed);
}

void testGlobalEnumWithoutTag(void) {
    ASSERT(0, GNamelessA);
    ASSERT(1, GNamelessB);
    ASSERT(2, GNamelessC);

    ASSERT(0, GNamelessX);
    ASSERT(1, GNamelessY);
    ASSERT(2, GNamelessZ);

    gNameless = GNamelessZ;
    ASSERT(2, gNameless);
}

void testLocalEnumWithTag(void) {
    {
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
    {
        enum B {
            ItemX,
            ItemY,
            ItemZ
        } v = ItemY;

        ASSERT(0, ItemX);
        ASSERT(1, ItemY);
        ASSERT(2, ItemZ);
        ASSERT(1, v);
    }
}

void testLocalEnumWithoutTag(void) {
    enum {
        NamelessA,
        NamelessB,
        NamelessC,
    } v = NamelessC;

    ASSERT(0, NamelessA);
    ASSERT(1, NamelessB);
    ASSERT(2, NamelessC);
    ASSERT(2, v);
}

void testSizeOfEnum(void) {
    enum A {
        ItemA,
        ItemB,
    };

    ASSERT(4, sizeof(enum A));
}

int main(void) {
    testGlobalEnumWithTag();
    testGlobalEnumWithoutTag();
    testLocalEnumWithTag();
    testLocalEnumWithoutTag();
    testSizeOfEnum();
    return 0;
}
