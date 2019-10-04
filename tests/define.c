#include <stdio.h>

#define n 4
#define eee for (i = 0; i < 8; i++)
#define xxx
#define if for

int main() {
    int i;
    if (xxx i = 0; i < n xxx; i++) {
        printf("%d\n", i);
    }
    eee {
        printf("%d\n", i);
    }
#undef if
#define a0() printf("%d\n", 42)
#define a1(x) printf("%d\n", x)
#define a2(x, y) printf("%d %d\n", x, y)
#define a3(x, y, z) x y z
    if (1) {
        a0();
        a1(4);
        a2(345, 240);
        a3(i, =, 639);
        a1(i);
    }
}
