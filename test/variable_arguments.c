#include "test.h"

typedef struct {
    int gp_offset;
    int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} va_list;
int vsprintf(char *, const char *, va_list);

char *fmt1(const char *fmt, ...) {
    static char buf[64];
    va_list ap;
    __builtin_va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    return buf;
}

void testVaStart_1(void) {
    char *result;

    result = fmt1("NO-VA_ARGS");
    ASSERT(0, strcmp(result, "NO-VA_ARGS"));

    result = fmt1("%s, %d", "Hello", 10);
    ASSERT(0, strcmp(result, "Hello, 10"));

    result = fmt1("%d, %d, %d, %d, %d, %d, %d", 101, 102, 103, 104, 105, 106, 107);
    ASSERT(0, strcmp(result, "101, 102, 103, 104, 105, 106, 107"));
}

int sprintf(char *, const char *, ...);
int strlen(const char*);
char *fmt2(int n1, int n2, int n3, int n4, int n5, int n6, const char *fmt, ...) {
    static char buf[128];
    va_list ap;

    sprintf(buf, "%d, %d, %d, %d, %d, %d: ", n1, n2, n3, n4, n5, n6);
    __builtin_va_start(ap, fmt);
    vsprintf(&buf[strlen(buf)], fmt, ap);
    return buf;
}

void testVaStart_2(void) {
    char *result;

    result = fmt2(11, 12, 13, 14, 15, 16, "NO-VA_ARGS");
    ASSERT(0, strcmp(result, "11, 12, 13, 14, 15, 16: NO-VA_ARGS"));
}

int main(void) {
    testVaStart_1();
    testVaStart_2();
}
