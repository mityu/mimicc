#define ASSERT(act, expr)    \
    do{ subassert(__FILE__, __LINE__, (act), (expr)); } while (0);

int strcmp(const char *, const char *);
int printf(const char *, ...);
_Noreturn void exit(int);
static void subassert(const char *file, int line, int actual, int got) {
    if (actual != got) {
        printf("%s:%d: Value mismatch: got: %d\n", file, line, got);
        exit(1);
    }
}

#define STR1(x) #x
static void testHashHashOperator(void) {
    ASSERT(0, strcmp(STR1(foo), "foo"));
    ASSERT(0, strcmp(STR1(10-bar), "10-bar"));
    ASSERT(0, strcmp(STR1('c'), "'c'"));
    ASSERT(0, strcmp(STR1('\n'), "'\\n'"));
    ASSERT(0, strcmp(STR1('\\'), "'\\\\'"));
    ASSERT(0, strcmp(STR1("baz"), "\"baz\""));
    ASSERT(0, strcmp(STR1("\n \\"), "\"\\n \\\\\""));
    ASSERT(0, strcmp(STR1(foo   bar), "foo bar"));
    ASSERT(0, strcmp(STR1(hoge/**/fuga), "hoge fuga"))
}

int main(void) {
    // Make sure macros works well.
    ASSERT(0, 0);
    ASSERT(1, 1);

    testHashHashOperator();
}
