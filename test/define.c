_Noreturn void exit(int status);
int printf(const char *fmt, ...);
int strcmp(const char *, const char *);

// Make sure no SEGV when deleting the only entry of macro.
#define UNDEF_SOON
#undef UNDEF_SOON

#define REP_TO_53   53
#define REP_TO_59   59
void testObjectiveMacro(void) {
    int n1 = REP_TO_53;
    int n2 = REP_TO_53;
    int n3 = REP_TO_59;
    char str[] = "REP_TO_53";

    if (n1 != 53) {
        printf("testObjectiveMacro(): n1 != 53: n1 is %d\n", n1);
        exit(1);
    }

    if (n2 != 53) {
        printf("testObjectiveMacro(): n2 != 53: n2 is %d\n", n2);
        exit(1);
    }

    if (n3 != 59) {
        printf("testObjectiveMacro(): n3 != 59: n3 is %d\n", n3);
        exit(1);
    }

    if (strcmp(str, "REP_TO_53") != 0) {
        printf("testObjectiveMacro(): string is wrongly replaced: %s\n", str);
        exit(1);
    }
}

#define REP_TO_EMPTY
void testEmptyObjectiveMacro(void) {
    int n = REP_TO_EMPTY 101 REP_TO_EMPTY;

    if (n != 101) {
        printf("testEmptyObjectiveMacro(): n != 101: n is %d\n", n);
        exit(1);
    }
}

#define REP_TO_MACRO    REP_TO_59
void testNestedObjectiveMacro(void) {
    int n = REP_TO_MACRO;

    if (n != 59) {
        printf("testObjectiveMacro(): n != 59: n is %d\n", n);
        exit(1);
    }
}

#define REP_TO_PREDEFINED_LINE  __LINE__
void test__LINE__macro(void) {
    int line = __LINE__;
    if (line != 60) {
        printf("test__LINE__macro(): Wrong line number (direct): %d\n", line);
        exit(1);
    }

    line = REP_TO_PREDEFINED_LINE;
    if (line != 66) {
        printf("test__LINE__macro(): Wrong line number (indirect): %d\n", line);
        exit(1);
    }
}

#define MULTI_EXPAND_1  (REP_TO_53*REP_TO_53)
#define MULTI_EXPAND_2  (REP_TO_53+MULTI_EXPAND_1)
void testMacroMultipleExpantion(void) {
    if (2809 != MULTI_EXPAND_1) {
        printf("testMacroMultipleExpantion(): MULTI_EXPAND_1 != 2809: %d\n",
                MULTI_EXPAND_1);
        exit(1);
    }

    if (2862 != MULTI_EXPAND_2) {
        printf("testMacroMultipleExpantion(): MULTI_EXPAND_2 != 2862: %d\n",
                MULTI_EXPAND_2);
        exit(1);
    }
}

#define UNDEF_TEST_MACRO    "foo-bar-baz"
#undef UNDEF_TEST_MACRO
#define UNDEF_TEST_MACRO    61
void testUndef(void) {
    if (UNDEF_TEST_MACRO != 61) {
        printf("testUndef(): Invalid UNDEF_TEST_MACRO value: %d\n", UNDEF_TEST_MACRO);
        exit(1);
    }
}

#define SUCC(x) (x + 1)
void testFuncLikeMacroWithOneParam(void) {
    int n;

    n = SUCC(3);
    if (n != 4) {
        printf("testFuncLikeMacroWithOneParam(): n != 4: %d", n);
        exit(1);
    }

    n = SUCC(SUCC(5));
    if (n != 7) {
        printf("testFuncLikeMacroWithOneParam(): n != 7: %d", n);
        exit(1);
    }

    n = SUCC(SUCC(SUCC(7)));
    if (n != 10) {
        printf("testFuncLikeMacroWithOneParam(): n != 10: %d", n);
        exit(1);
    }
}

int main(void) {
    testObjectiveMacro();
    testEmptyObjectiveMacro();
    testNestedObjectiveMacro();
    test__LINE__macro();
    testMacroMultipleExpantion();
    testUndef();
    testFuncLikeMacroWithOneParam();
    printf("OK\n");
}
