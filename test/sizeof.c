#include "test.h"

int gar1[2];
int gar2[2][3];
int gar3[2][3][4];

int main(void) {
    int n;
    int *p;
    int ar1[2];
    int ar2[2][3];
    int ar3[2][3][4];

    ASSERT(4, sizeof(10));
    ASSERT(4, sizeof(n));
    ASSERT(4, sizeof(sizeof(n)));
    ASSERT(8, sizeof(p));
    ASSERT(8, sizeof p);
    ASSERT(4, sizeof(*p));
    ASSERT(4, sizeof(int));
    ASSERT(8, sizeof(int *));
    ASSERT(8, sizeof(int **));
    ASSERT(12, sizeof(int[3]));
    ASSERT(60, sizeof(int[3][5]));
    ASSERT(24, sizeof(int*[3]));
    ASSERT(24, sizeof(int**[3]));
    ASSERT(1, sizeof(char));
    ASSERT(8, sizeof(char *));
    ASSERT(8, sizeof(char **));
    ASSERT(1, sizeof(void));
    ASSERT(8, sizeof(void *));
    ASSERT(8, sizeof(void **));
    ASSERT(4, sizeof(ar1[0]));
    ASSERT(8, sizeof(ar1));
    ASSERT(24, sizeof(ar2));
    ASSERT(12, sizeof(ar2[0]));
    ASSERT(16, sizeof(ar3[0][0]));
    ASSERT(4, sizeof(gar1[0]));
    ASSERT(8, sizeof(gar1));
    ASSERT(24, sizeof(gar2));
    ASSERT(12, sizeof(gar2[0]));
    ASSERT(16, sizeof(gar3[0][0]));
    ASSERT(4, sizeof("foo"));
    ASSERT(2, sizeof("\n"));
    ASSERT(2, sizeof("\t"));
    ASSERT(2, sizeof("\\"));
    ASSERT(2, sizeof("\""));

    return 0;
}
