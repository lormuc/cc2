#include <stdio.h>

int main() {
    enum a { a, b = 3, c = b, d, e } x;
    enum a y, *z;
    printf("%d %d %d %d %d\n", a, b, c, d, e);
    z = &y;
    x = a;
    y = x;
    printf("%d\n", x + 9);
    printf("%d\n", y);
    printf("%d\n", *z * 2);
    sizeof(enum {aa = 4});
    printf("%d\n", aa);
}
