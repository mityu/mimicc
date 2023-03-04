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

int test_for_break_1(void) {
    int n = 7;
    for (;;) {
        break;
        n = 5;
    }
    return n;
}

int test_for_break_2(void) {
    int n = 7;
    for (;;)
        break;
    n = 5;
    return n;
}

int test_for_break_3(void) {
    int n = 7;
    for (;;) {
        n++;
        break;
    }
    return n;
}

int test_for_break_4(void) {
    int n = 11;
    for (int i = 0; i < 1; ++i) {
        for (int j = 0; j < 3; ++j) {
            n = 7;
            break;
        }
        n = 5;
    }
    return n;
}

int test_for_break_5(void) {
    int n = 11;
    if (1) {
        for (;;) {
            n = 7;
            break;
        }
        n = 5;
    }
    return n;
}

int test_for_break_6(void) {
    int n[3] = {1, 1, 1};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 3; ++k) {
                ++n[2];
            }
            ++n[1];
            break;
        }
        ++n[0];
        break;
    }
    return n[0] * 10000 + n[1] * 100 + n[2];
}

void test_for_continue_1(void) {
    int i, n;
    n = 7;
    for (i = 0; i < 5; ++i) {
        continue;
        n = 13;
    }
    ASSERT(5, i);
    ASSERT(7, n);
}

void test_for_continue_2(void) {
    int i, j, n;
    n = 13;
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 5; ++j) {
            continue;
            n = 17;
        }
    }
    ASSERT(13, n);
    ASSERT(3, i);
    ASSERT(5, j);
}

void test_for_continue_3(void) {
    int i, j, n;
    n = 13;
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            n = 17;
            continue;
            n = 19;
        }
    }

    ASSERT(17, n);
    ASSERT(3, i);
    ASSERT(3, j);
}

void test_for_continue_4(void) {
    int flag_stay = 1;
    int n = 13;
    for (; flag_stay;) {
        n++;
        flag_stay = 0;
        continue;
    }
    ASSERT(14, n);
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

void test_do_while(void) {
    int n = 5;
    ASSERT(5, n);
    do {
        n += 2;
    } while(0);
    ASSERT(7, n);
}

void test_do_while_break(void) {
    int n = 5;
    ASSERT(5, n);
    do {
        break;
        n += 2;
    } while(0);
    ASSERT(5, n);
}

void test_do_while_continue_1(void) {
    int n = 5, m = 13;
    int count = 0;
    ASSERT(5, n);
    do {
        count++;
        if (count <= 2)
            continue;
        m += 5;
        break;
        n += 2;
    } while(1);
    ASSERT(5, n);
    ASSERT(18, m);
}

void test_do_while_continue_2(void) {
    int n = 13;
    do {
        n++;
        continue;
    } while(0);
    ASSERT(14, n);
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

void test_cond_not(void) {
    int a = 3;
    int n = 7;

    ASSERT(3, a);
    if (!a) {
        n = 11;
    } else {
        n = 13;
    }
    ASSERT(13, n);

    a = 0;
    ASSERT(0, a);
    if (!a) {
        n = 17;
    } else {
        n = 19;
    }
    ASSERT(17, n);
}

// Not operator didn't push result to stack and this function got SEGV.
void test_not(void) {
    int p = 3;
    if (!0 && !0 && !0) p = 5;
    ASSERT(5, p);
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
    ASSERT(7, test_for_break_1());
    ASSERT(5, test_for_break_2());
    ASSERT(8, test_for_break_3());
    ASSERT(5, test_for_break_4());
    ASSERT(5, test_for_break_5());
    ASSERT(20204, test_for_break_6());
    test_for_continue_1();
    test_for_continue_2();
    test_for_continue_3();
    test_for_continue_4();
    ASSERT(10, test_while_return_1());
    ASSERT(10, test_while_1());
    ASSERT(50, test_while_2());
    ASSERT(20, test_while_3());
    test_do_while();
    test_do_while_break();
    test_do_while_continue_1();
    test_do_while_continue_2();
    test_comma_1();
    test_comma_2();
    test_cond_not();
    test_not();
    return 0;
}
