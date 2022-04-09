#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Invalid arguments\n");
        return 1;
    }

    puts(".intel_syntax noprefix");
    puts(".globl main");
    puts("main:");
    printf("  mov rax, %d\n", atoi(argv[1]));
    puts("  ret");
    return 0;
}
