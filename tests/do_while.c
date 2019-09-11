#include <stdio.h>

int main() {
    int i = 0;
    do {
        i = i + 1;
        printf("%d\n", i);
    } while (i < 5);
    int k = 0;
    do k = k + 1; while (k < 10);
    printf("%d\n", k);
    int j = 0;
    do {
        if (j < 5) {
            printf("%d\n", j);
        } else {
            break;
        }
        j = j + 1;
        continue;
        j = j + 1;
    } while (1);
}
