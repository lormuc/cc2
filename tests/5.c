#include <stdio.h>

int main() {
    int z;
    z = 4;
    int* w;
    w = &z;
    *w = 9;
    printf("%d\n", z);
}
