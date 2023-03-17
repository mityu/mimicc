#include "test.h"

#define unreachable()   \
    do {\
        printf("%s:%d: unreachable\n", __FILE__, __LINE__);\
    } while(0)
#define ALREADY_DEFINED

void testPrimary(void) {
    int n;

#if 1
#elif 1
#else
#endif


    n = 0;
    ASSERT(0, n);
#if 0
    unreachable();
#else
    n = 13;
#endif
    ASSERT(13, n);

    n = 0;
    ASSERT(0, n);
#if 0
    unreachable();
#elif 0
    unreachable();
#elif 0
    unreachable();
#elif 1
    n = 17;
#else
    unreachable();
#endif
    ASSERT(17, n);

    n = 0;
    ASSERT(0, n);
#if 0
    unreachable();
#elif 0
    unreachable();
#elif 0
    unreachable();
#else
    n = 19;
#endif
    ASSERT(19, n);

    n = 0;
    ASSERT(0, n);
#if 1
    n = 23;
#elif 1
    unreachable();
#else
    unreachable();
#endif
    ASSERT(23, n);

#if defined(NOT_DEFINED)
    unreachable();
#elif defined(ALREADY_DEFINED)
    n = 29;
#endif
    ASSERT(29, n);

#define FN_DEFINED(x)   x
#if defined(FN_DEFINED)
    n = 31;
#endif
    ASSERT(31, n);
}

void testIfCond(void) {
    int n = 0;
    ASSERT(0, n);
#if !0
    n = 3;
#endif
    ASSERT(3, n);

#if +1
    n = 5;
#endif
    ASSERT(5, n);

#if -+0
    unreachable();
#endif

#if 13 * 17 * 23 * 0
    unreachable();
#endif

#if 5 / 2 / 2
    n = 7;
#endif
    ASSERT(7, n);

#if 0 / 13 / 23
    unreachable();
#endif

#if 9 % 3
    unreachable();
#endif

#if 8 % 3
    n = 9;
#endif
    ASSERT(9, n);

#if 3 + 5 + 7 - 15
    unreachable();
#endif

#if 3 + 9
    n = 11;
#endif
    ASSERT(11, n);

#if 15 - 7 - 8
    unreachable();
#endif

#if 1 << 2
    n = 13;
#endif
    ASSERT(13, n);

#if 0 << 3
    unreachable();
#endif

#if 0 >> 3 >> 2
    unreachable();
#endif

#if 1 >> 2
    unreachable();
#endif

#if 3 > 3
    unreachable();
#endif

#if 3 >= 4
    unreachable();
#endif

#if 3 < 3
    unreachable();
#endif

#if 4 <= 3
    unreachable();
#endif

#if 2 >= 1
    n = 17;
#endif
    ASSERT(17, n);

#if 2 == 3
    unreachable();
#endif

#if 3 != 3
    unreachable();
#endif

#if 5 == 2 + 3
    n = 19;
#endif
    ASSERT(19, n);

#if 123 & 0
    unreachable();
#endif

#if 0 | 1
    n = 23;
#endif
    ASSERT(23, n);

#if 0 | 2 & 3
    n = 29;
#endif
    ASSERT(29, n);

#if 0 ^ 3 ^ 32
    n = 31;
#endif
    ASSERT(31, n);

#if 1 && 0
    unreachable();
#elif 1 && 2
    n = 37;
#endif
    ASSERT(37, n);

#if 1 || 0 && 0
    n = 41;
#endif
    ASSERT(41, n);

#if 0 ? 1 : 0
    unreachable();
#elif 1 ? 2 - 2 : 3
    unreachable();
#elif 1 ? 2 ? 0 : 3 : 4
    unreachable();
#elif 3 ? 4 + 5 : 1 + 2 - 3
    n = 43;
#endif
    ASSERT(43, n);

#if (2 + 0) * 3
    n = 47;
#endif
    ASSERT(47, n);

#if !defined(NOT_DEFINED)
    n = 53;
#endif
    ASSERT(53, n);

#if !(1 ? 2 - 2 : 3 + 5)
    n = 59;
#endif
    ASSERT(59, n);

#if defined(ALREADY_DEFINED) && defined(NOT_DEFINED)
    unreachable();
#elif defined(ALREADY_DEFINED) && !defined(NOT_DEFINED)
    n = 61;
#endif
    ASSERT(61, n);
}

int main(void) {
    testPrimary();
    testIfCond();
}
