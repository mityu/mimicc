#include "test.h"

typedef char* GCharPtr;
GCharPtr gCharPtr;

void testGlobalTypedefPrimitive(void) {
    char c = 7;
    gCharPtr = &c;

    ASSERT(7, c);
    ASSERT(7, *gCharPtr);

    *gCharPtr = 19;
    ASSERT(19, c);

    ASSERT(8, sizeof(GCharPtr));
    ASSERT(8, sizeof(gCharPtr));
}

void testTypedefPrimitive(void) {
    typedef int* IntPtr;
    int n = 13;
    IntPtr p = &n;

    ASSERT(13, n);
    ASSERT(13, *p);

    *p = 17;
    ASSERT(17, n);

    ASSERT(8, sizeof(IntPtr));
    ASSERT(8, sizeof(p));
}

void testTypedefStruct(void) {
    struct Struct1 {
        int n;
        int m;
    };
    typedef struct Struct1 S1;

    typedef struct Struct2 S2;
    struct Struct2 {
        S1 s1;
    };

    {
        S1 s1;
        s1.n = 5;
        s1.m = 7;

        ASSERT(5, s1.n);
        ASSERT(7, s1.m);
    }

    {
        S2 s2;
        s2.s1.n = 11;
        s2.s1.m = 13;

        ASSERT(11, s2.s1.n);
        ASSERT(13, s2.s1.m);
    }

    {
        S1 s1 = {19};
        S2 s2 = {{23, 29}};
        ASSERT(19, s1.n);
        ASSERT(0, s1.m);
        ASSERT(23, s2.s1.n);
        ASSERT(29, s2.s1.m);
    }

    ASSERT(8, sizeof(S1));
    ASSERT(8, sizeof(S2));
}

void testTypedefStructInPlace(void) {
    typedef struct {
        int n;
        int m;
    } S1;

    typedef struct {
        S1 s1;
    } S2;

    {
        S1 s1;
        s1.n = 5;
        s1.m = 7;

        ASSERT(5, s1.n);
        ASSERT(7, s1.m);
    }

    {
        S2 s2;
        s2.s1.n = 11;
        s2.s1.m = 13;

        ASSERT(11, s2.s1.n);
        ASSERT(13, s2.s1.m);
    }

    {
        S1 s1 = {19};
        S2 s2 = {{23, 29}};
        ASSERT(19, s1.n);
        ASSERT(0, s1.m);
        ASSERT(23, s2.s1.n);
        ASSERT(29, s2.s1.m);
    }

    ASSERT(8, sizeof(S1));
    ASSERT(8, sizeof(S2));
}

void testTypedefEnum(void) {
    typedef enum Enum1 E1;
    enum Enum1 {
        ItemA,
        ItemB,
    };

    E1 v = ItemB;

    ASSERT(1, v);
    ASSERT(4, sizeof(enum Enum1));
    ASSERT(4, sizeof(E1));
}

void testTypedefEnumInPlace(void) {
    typedef enum {
        ItemX,
        ItemY,
        ItemZ
    } Enum;

    Enum v = ItemZ;

    ASSERT(2, v);
    ASSERT(4, sizeof(Enum));
}

void testTypedefIncompleteArray(void) {
    typedef int Int[];
    Int a1 = {3, 5};
    Int a2 = {7};

    ASSERT(3, a1[0]);
    ASSERT(5, a1[1]);
    ASSERT(7, a2[0]);
    ASSERT(8, sizeof(a1));
    ASSERT(4, sizeof(a2));
}

int main(void) {
    testGlobalTypedefPrimitive();
    testTypedefPrimitive();
    testTypedefStruct();
    testTypedefStructInPlace();
    testTypedefEnum();
    testTypedefEnumInPlace();
    return 0;
}
