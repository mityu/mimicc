#include "test.h"

int main(void) {
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
    return 0;
}
