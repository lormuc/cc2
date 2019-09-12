#include <stdio.h>

int main() {
    int a[3] = {1, 4, 3};
    printf("%d\n", a < &a[1]);
    printf("%d\n", a <= &a[1]);
    printf("%d\n", a > &a[1]);
    printf("%d\n", a >= &a[1]);
    printf("%d\n", 1 < 9 < 3);
    printf("%d\n", -3 < -4);
    printf("%d\n", -3 > -4);
    printf("%d\n", -3 <= -4);
    printf("%d\n", -3 >= -4);
    printf("%d\n", 3 >= -4);
}
