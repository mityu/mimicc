#include <stdio.h>
#include <string.h>

// String copy function with Duff's Device optimization.
void duffsdevice(const char *src, char *dst) {
    switch (strlen(src) & 0x7) {
    case 0: do { *dst++ = *src++;
    case 7: *dst++ = *src++;
    case 6: *dst++ = *src++;
    case 5: *dst++ = *src++;
    case 4: *dst++ = *src++;
    case 3: *dst++ = *src++;
    case 2: *dst++ = *src++;
    case 1: *dst++ = *src++;
        } while (*src);
    }
}

#define BUFSIZE (1024)

int main(void) {
    const char src[BUFSIZE] = "Hello, world";
    char dst[BUFSIZE] = {};

    duffsdevice(src, dst);

    printf("src: %s\n", src);
    printf("dst: %s\n", dst);

    if (strcmp(src, dst) == 0)
        puts("Successfully copied.");
    else
        puts("Copy string failed.");
}
