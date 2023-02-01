#include "test.h"

int g_counter;

int inc_counter_true(void) {
    g_counter++;
    return 1;
}

int inc_counter_false(void) {
    g_counter++;
    return 0;
}

void test_logicalAND(void) {
    int n = 0;

    if (1 && 1) {
        // Do nothing
    } else {
        UNREACHABLE();
    }

    if (1 && 1 && 1) {
        // Do nothing
    } else {
        UNREACHABLE();
    }

    if (0 && 0)
        UNREACHABLE();

    if (0 && 1)
        UNREACHABLE();

    if (1 && 0)
        UNREACHABLE();

    if (1 && 0 && 1)
        UNREACHABLE();

    if (1 && 1 && 0)
        UNREACHABLE();

    if ((n = 3) && (n = 5) && (n = 7)) {
    } else {
        UNREACHABLE();
    }
    ASSERT(7, n);
    if ((n = 13) && (n = 0) && (n = 17)) {
        UNREACHABLE();
    }
    ASSERT(0, n);
}

void test_logicalAND_short_circuit(void) {
    g_counter = 0;
    ASSERT(0, g_counter);
    if (inc_counter_false() && inc_counter_false()) {
        UNREACHABLE();
    }
    ASSERT(1, g_counter);

    g_counter = 0;
    ASSERT(0, g_counter);
    if (inc_counter_false() && inc_counter_false() && inc_counter_false()) {
        UNREACHABLE();
    }
    ASSERT(1, g_counter);
}

void test_logicalOR(void) {
    int n = 0;

    if (1 || 1) {
        // Do nothing
    } else {
        UNREACHABLE();
    }

    if (1 || 0) {
        // Do nothing
    } else {
        UNREACHABLE();
    }


    if (0 || 1) {
        // Do nothing
    } else {
        UNREACHABLE();
    }

    if (0 || 0) {
        UNREACHABLE();
    }

    if (0 || 0 || 1) {
    } else {
        UNREACHABLE();
    }
}

void test_logicalOR_short_circuit(void) {
    g_counter = 0;
    ASSERT(0, g_counter);
    if (inc_counter_true() || inc_counter_false()) {
    } else {
        UNREACHABLE();
    }
    ASSERT(1, g_counter);

    g_counter = 0;
    ASSERT(0, g_counter);
    if (inc_counter_false() || inc_counter_false()) {
        UNREACHABLE();
    }
    ASSERT(2, g_counter);

    g_counter = 0;
    ASSERT(0, g_counter);
    if (inc_counter_false() || inc_counter_true() || inc_counter_false()) {
    } else {
        UNREACHABLE();
    }
    ASSERT(2, g_counter);
}

void test_logical_AND_OR_mixtured(void) {
    g_counter = 0;
    ASSERT(0, g_counter);
    if (inc_counter_true() && inc_counter_false() ||
            inc_counter_true() && inc_counter_true()) {
    } else {
        UNREACHABLE();
    }
    ASSERT(4, g_counter);

    g_counter = 0;
    ASSERT(0, g_counter);
    if (inc_counter_false() || inc_counter_true() && inc_counter_true()) {
    } else {
        UNREACHABLE();
    }
    ASSERT(3, g_counter);
}

int main(void) {
    test_logicalAND();
    test_logicalAND_short_circuit();
    test_logicalOR();
    test_logicalOR_short_circuit();
    test_logical_AND_OR_mixtured();
    return 0;
}
