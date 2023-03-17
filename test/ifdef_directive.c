#define ALREADY_DEFINED

int printf(const char *, ...);

#define UNREACHABLE()  do {printf("%d: unreachable\n", __LINE__);} while(0)

void testIfdefIfndef(void) {
    int n;

    n = 0;
    if (n != 0)
        UNREACHABLE();
#ifdef ALREADY_DEFINED
    n = 13;
#endif
    if (n != 13)
        UNREACHABLE();

#ifndef ALREADY_DEFINED
    UNREACHABLE();
#endif


#ifdef NOT_DEFINED
    UNREACHABLE();
#endif

    n = 0;
    if (n != 0)
        UNREACHABLE();
#ifndef NOT_DEFINED
    n = 17;
#endif
    if (n != 17)
        UNREACHABLE();
}

void testHeaderGuard(void) {
    int n = 0;
#ifndef HEADER_GUARD
#define HEADER_GUARD
    n = 19;
#endif

#ifndef HEADER_GUARD
#define HEADER_GUARD
    UNREACHABLE();
#endif

    if (n != 19)
        UNREACHABLE();
}

int main(void) {
    testIfdefIfndef();
    testHeaderGuard();
}
