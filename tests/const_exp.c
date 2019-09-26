#include <stdio.h>

int main() {
    enum { a = 4, b = a *2, c = b*2, d = 5 + a, e = c << 2 };
    int aa[4 + 4 * 4/2 + (15 % 6) + e + d];
    int bb[(7 & 3) + (13 ^ 56) + (2 | 1) + (3 << 2) + (-1 >> 1) + ~-1];
    int cc[(3 < 4) + (4 < 3) + (5 == 7) + (0 == 0)];
    printf("%lu\n", sizeof(aa));
    printf("%lu\n", sizeof(bb));
    printf("%lu\n", sizeof(cc));
}
