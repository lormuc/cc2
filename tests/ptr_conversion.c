#include <stdio.h>

int main() {
    void* x;
    int z;
    int* y;
    x = 0;
    x = &z;
    y = x;
    *y = 4;
    printf("%d\n", z);
}
