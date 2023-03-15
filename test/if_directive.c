#define ALREADY_DEFINED

int printf(const char *, ...);

#define UNREACHABLE()  do {printf("%d: unreachable\n", __LINE__);} while(0)

int main(void) {
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
    n = 23;
#endif
    if (n != 23)
        UNREACHABLE();
}
