#include "test.h"

int test_if_return_1(void) {
    if (1)
        return 42;
    return 10;
}

int test_if_return_2(void) {
    if (0)
        return 10;
    return 42;
}

int test_if_return_3(void) {
    if (1)
        if(1)
            return 42;
    return 10;
}

int test_if_return_4(void) {
    if (1)
        if(0)
            return 10;
    return 42;
}

int test_if_return_5(void) {
    if (0)
        return 10;
    else
        return 42;
    return 20;
}

int test_if_return_6(void) {
    if (0)
        return 10;
    else if (1)
        return 42;
    else
        return 20;
    return 30;
}

int test_if_return_7(void) {
    if (0)
        return 10;
    else if (0)
        return 20;
    else if (1)
        return 42;
    else
        return 30;
    return 40;
}

int test_if_return_8(void) {
    if (0)
        return 10;
    else if (0)
        return 20;
    else if (0)
        return 30;
    else
        return 42;
    return 40;
}

int test_if_return_9(void) {
    if (0)
        return 10;
    else if (0)
        return 20;
    else if (0)
        return 30;
    return 42;
}


int test_for_return_1(void) {
    for (;;)
        return 10;
}

int test_for_return_2(void) {
    int i;
    for (i = 1;;)
        return 10;
}

int test_for_return_3(void) {
    for (; 1;)
        return 10;
}

int test_for_return_4(void) {
    for (;; 1)
        return 10;
}

int test_for_1(void) {
    int a, i;
    a = 0;
    for (i = 0; i < 3; ++i)
        a = a + i + 1;
    return a;
}

int test_for_2(void) {
    int a, b, i;
    a = b = 1;
    for (i = 0; i < 9; ++i) {
        int tmp;
        tmp = a;
        a = a + b;
        b = tmp;
    }
    return a;
}

int test_for_3(void) {
    int sum = 0;
    for (int i = 1; i <= 10; ++i) {
        sum += i;
    }
    return sum;
}

int test_while_return_1(void) {
    while(1)
        return 10;
}

int test_while_1(void) {
    while(0)
        return 3;
    return 10;
}

int test_while_2(void) {
    int a;
    a = 0;
    while (a < 1)
        while (a < 2)
            a++;
    return 50;
}

int test_while_3(void) {
    int a;
    a = 0;
    while (a < 10)
        while (a < 20)
            a++;
    return a;
}

// TODO: These should be in another file?
int helper_always_1(void) {
    return 1;
}

int helper_always_3(void) {
    return 3;
}

void test_comma_1(void) {
    int n[] = {5, 7, 11, 13, 17, 19};
    int r;
    r = n[helper_always_1(), helper_always_3()];
    ASSERT(13, r);
    r = n[helper_always_3(), helper_always_1()];
    ASSERT(7, r);
}

void test_comma_2(void) {
    int a = 3, b = 5;
    int c;
    c = a = 11, b = 13;
    ASSERT(11, a);
    ASSERT(13, b);
    ASSERT(11, c);
}

int main(void) {
    ASSERT(42, test_if_return_1());
    ASSERT(42, test_if_return_2());
    ASSERT(42, test_if_return_3());
    ASSERT(42, test_if_return_4());
    ASSERT(42, test_if_return_5());
    ASSERT(42, test_if_return_6());
    ASSERT(42, test_if_return_7());
    ASSERT(42, test_if_return_8());
    ASSERT(42, test_if_return_9());
    ASSERT(10, test_for_return_1());
    ASSERT(10, test_for_return_2());
    ASSERT(10, test_for_return_3());
    ASSERT(10, test_for_return_4());
    ASSERT(6, test_for_1());
    ASSERT(89, test_for_2());
    ASSERT(55, test_for_3());
    ASSERT(10, test_while_return_1());
    ASSERT(10, test_while_1());
    ASSERT(50, test_while_2());
    ASSERT(20, test_while_3());
    test_comma_1();
    test_comma_2();
    return 0;
}
