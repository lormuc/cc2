#include <stdio.h>

int main() {
    int x = 0;
    if (1) if (0) x = 1; else x = 2;
    printf("%d\n", x);
}
