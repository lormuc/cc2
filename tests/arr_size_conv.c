#include <stdio.h>

int main() {
    int a[(unsigned long)(7.5f)];
    int b[(unsigned long)(-1) + 2];
    printf("%lu\n", sizeof(a) / sizeof(*a));
    printf("%lu\n", sizeof(b) / sizeof(*b));
    printf("%lu\n", sizeof(int [4+4]));
    printf("%lu\n", sizeof(int (**[7][6])[5][4]));
}
