#include <stdio.h>

int main() {
    int i = 0, j = 5;
    for (int i = 0, j; i < 5; i++) {
        int i = 9;
        printf("%d\n", i);
        j = 10;
    }
    printf("%d\n", i);
    printf("%d\n", j);
}
