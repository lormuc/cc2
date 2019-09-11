#include <stdio.h>

int main() {
    int i, j;
    float k;
    for (i = 0; i < 4; i++) {
        printf("%d\n", i);
    }
    i = 0;
    j = i+++3;
    printf("%d\n", i);
    printf("%d\n", j);
    k = 1;
    k++;
    printf("%f\n", k);
}
