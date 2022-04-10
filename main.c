#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Invalid arguments\n");
        return 1;
    }

    char *p = argv[1];

    puts(".intel_syntax noprefix");
    puts(".globl main");
    puts("main:");
    printf("  mov rax, %ld\n", strtol(p, &p, 10));

    while (*p) {
        if (*p == '+') {
            ++p;
            printf("  add rax, %ld\n", strtol(p, &p, 10));
            continue;
        } else if (*p == '-') {
            ++p;
            printf("  sub rax, %ld\n", strtol(p, &p, 10));
            continue;
        }

        fprintf(stderr, "Unexpected character: '%c'\n", *p);
        return 1;
    }

    puts("  ret");
    return 0;
}
