#include <stdio.h>

typedef int x;

int main() {
    int z = 1;
    int* y = &z;
    enum { x = 4 };
    printf("%d\n", (x)**y);
}
