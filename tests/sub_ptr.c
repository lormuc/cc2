#include <stdio.h>

int main() {
    int a[4] = {1, 2, 3, 4};
    printf("%ld\n", &a[3] - &a[2]);
    printf("%ld\n", &a[3] - &a[0]);
    printf("%ld\n", &a[1] - &a[2]);
}
