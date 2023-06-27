#include <stdio.h>
#include <stdlib.h>

void movePlate(int count, int src, int dest);
void printHanoi(void);

struct Poll {
    int *plates;
    int height;
};

struct Poll hanoi[3];
int maxHeight;

void solveHanoi(int height) {
    maxHeight = height;
    for (int i = 0; i < 3; ++i) {
        hanoi[i].plates = malloc(height * sizeof(int));
        if (!hanoi[i].plates) {
            printf("Allocating memory failed\n");
            exit(1);
        }
        hanoi[i].height = 0;
        for (int h = 0; h < height; ++h)
            hanoi[i].plates[h] = 0;
    }
    hanoi[0].height = height;
    for (int i = 0; i < height; ++i) {
        hanoi[0].plates[i] = 2 * (height - i) - 1;
    }
    printHanoi();
    movePlate(height, 0, 2);
}

void movePlate(int count, int src, int dest) {
    if (count == 1) {
        int srcTop, destTop;
        hanoi[dest].plates[hanoi[dest].height] =
            hanoi[src].plates[hanoi[src].height - 1];
        hanoi[src].plates[hanoi[src].height - 1] = 0;
        hanoi[src].height--;
        hanoi[dest].height++;
        printHanoi();
    } else {
        int rest = 3 - src - dest;
        movePlate(count - 1, src, rest);
        movePlate(1, src, dest);
        movePlate(count - 1, rest, dest);
    }
}

void printHanoi(void) {
    int maxWidth = 2 * maxHeight - 1;

    printf("\n");

    for (int i = maxHeight - 1; i >= 0; --i) {
        for (int poll = 0; poll < 3; ++poll) {
            if (hanoi[poll].height > i) {
                int padding = (maxWidth - hanoi[poll].plates[i]) / 2;
                printf("%*s", padding + 2, " ");
                for (int cnt = 0; cnt < hanoi[poll].plates[i]; ++cnt) {
                    putchar('*');
                }
                printf("%*s", padding + 1, " ");
            } else {
                printf("%*s", maxWidth + 3, " ");
            }
        }
        printf("\n");
    }

    for (int i = 0; i < maxWidth * 3 + 9; ++i) {
        putchar('-');
    }
}

int main(void) {
    solveHanoi(4);
    return 0;
}
