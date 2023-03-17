#include "test.h"

void testArithmeticComputation(void) {
    ASSERT(4, 5-3+2);
    ASSERT(6, 5+3-2);
    ASSERT(3, 6/2);
    ASSERT(3, 7/2);
    ASSERT(3, 8%5);
    ASSERT(11, 5+3*2);
    ASSERT(7, 5+4/2);
    ASSERT(2, 10-4*2);
    ASSERT(8, 10-4/2);
    ASSERT(10, 15/3*2);
    ASSERT(10, 15*2/3);
    ASSERT(16, (5+3)*2);
    ASSERT(17, (5+3)*2+1);
    ASSERT(24, (5+3)*(2+1));
    ASSERT(4, (5+3)/2);
    ASSERT(5, (5+3)/2+1);
    ASSERT(2, (5+3)/(2+1));
    ASSERT(10, -10+20);
    ASSERT(10, -5*+10+60);
    ASSERT(75, -(-5*+15));
    ASSERT(1, 1 == 1);
    ASSERT(0, 0 == 1);
    ASSERT(1, 0 != 1);
    ASSERT(0, 1 != 1);
    ASSERT(1, 1 < 2);
    ASSERT(0, 3 < 2);
    ASSERT(1, 1 <= 2);
    ASSERT(1, 2 <= 2);
    ASSERT(0, 3 <= 2);
    ASSERT(0, 1 > 2);
    ASSERT(1, 3 > 2);
    ASSERT(0, 1 >= 2);
    ASSERT(1, 2 >= 2);
    ASSERT(1, 3 >= 2);
    ASSERT(1, 1 < 1 << 2);
    ASSERT(1, 2 > 2 >> 1);
}

void testBitwiseOperations(void) {
    ASSERT(1, 3 & 5);
    ASSERT(7, 3 | 5);
    ASSERT(6, 3 ^ 5);
    ASSERT(5, 7 & 6 ^ 3);
    ASSERT(3, 3 & 6 | 1 & 1);
    ASSERT(0, 3 | 7 ^ 7);
    ASSERT(2, 1 << 1);
    ASSERT(32, 1 << 5);
    ASSERT(256, 1 << 8);
    ASSERT(1, 2 >> 1);
    ASSERT(8, 64 >> 3);
    ASSERT(-2, -8 >> 2);
    ASSERT(8, 1 << 1 << 2);
    ASSERT(16, 1 << (1 << 2));
}

void testArithmeticAssignment(void) {
    int n;

    n = 13;
    n *= 23;
    ASSERT(299, n);

    n = 39;
    n /= 3;
    ASSERT(13, n);

    n = 3;
    n &= 5;
    ASSERT(1, n);

    n = 3;
    n |= 5;
    ASSERT(7, n);

    n = 3;
    n ^= 5;
    ASSERT(6, n);

    n = 2;
    n <<= 2;
    ASSERT(8, n);

    n = 1;
    n <<= 8;
    ASSERT(256, n);

    n = 2;
    n >>= 1;
    ASSERT(1, n);

    n = 64;
    n >>= 3;
    ASSERT(8, n);

    n = -8;
    n >>= 2;
    ASSERT(-2, n);
}

int main(void) {
    testArithmeticComputation();
    testArithmeticAssignment();
    return 0;
}
