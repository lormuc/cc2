#include <stdio.h>

int main() {
    struct { int xx; int yy; } a[10];
    int i;
    for (i = 0; i < 10; i = i + 1) {
        a[i].yy = i;
    }
    int sum;
    sum = 0;
    for (i = 0; i < 10; i = i + 1) {
        sum = sum + a[i].yy;
    }
    printf("%d\n", sum);
}
