#include "include.header"

_Noreturn void exit(int);
int printf(const char *, ...);

void test__LINE__macroInHeader(void) {
    int n = REP_TO_LINE_MACRO;
    if (n != 7) {
        printf("test__LINE__macroInHeader(): n != 7: %d\n", n);
        exit(1);
    }
}

void testObjectLikeMacro(void) {
    int n;

    n = REP_TO_41;
    if (n != 41) {
        printf("testObjectLikeMacro(): n != 41: %d\n", n);
        exit(1);
    }

    n = REP_TO_43;
    if (n != 43) {
        printf("testObjectLikeMacro(): n != 43: %d\n", n);
        exit(1);
    }

    n = REP_TO_MACRO_41;
    if (n != 41) {
        printf("testObjectLikeMacro(): n != 41: %d\n", n);
        exit(1);
    }
}

void testFunctionLikeMacro(void) {
    int n;

    n = PLUS(3, 5);
    if (n != 8) {
        printf("testFunctionLikeMacro(): n != 8: %d\n", n);
        exit(1);
    }

    n = PLUS(REP_TO_43, REP_TO_MACRO_41);
    if (n != 84) {
        printf("testFunctionLikeMacro(): n != 84: %d\n", n);
        exit(1);
    }

    n = PLUS_WITH_MACRO(REP_TO_41);
    if (n != 82) {
        printf("testFunctionLikeMacro(): n != 82: %d\n", n);
        exit(1);
    }
}

void testFuncCall(void) {
    int n = ret47();

    if (n != 47) {
        printf("testFuncCall(): n != 47: %d\n", n);
        exit(1);
    }
}

void testStructDefinition(void) {
    struct GSH obj = {3};
    if (obj.num != 3) {
        printf("testStructDefinition(): obj.num != 3: %d\n", obj.num);
        exit(1);
    }

    int n = retElemOfGSH(&obj);
    if (n != 3) {
        printf("testStructDefinition(): n != 3: %d\n", n);
        exit(1);
    }
}

void testTypedef(void) {
    struct GSH src = {7};
    GSH dest;

    dest = src;
    if (dest.num != 7) {
        printf("testTypedef(): dest.num != 7: %d\n", dest.num);
        exit(1);
    }
}

int main(void) {
    test__LINE__macroInHeader();
    testObjectLikeMacro();
    testFunctionLikeMacro();
    testFuncCall();
    testStructDefinition();
    testTypedef();
}
