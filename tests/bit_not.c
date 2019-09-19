#include <stdio.h>

int main() {
    unsigned x = 4;
    int y = 7;
    unsigned long z = 34;
    long w = 1349;
    printf("%d", ~y + y + 1);
    printf("%u", ~x + x + 1);
    printf("%ld", ~w + w + 1);
    printf("%lu", ~z + z + 1);
}
