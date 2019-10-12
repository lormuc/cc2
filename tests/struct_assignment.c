#include <stdio.h>

int main() {
    struct s0 {
        int a;
        int b;
        int c;
    };
    struct s0 y;
    y.a = 4;
    y.b = 5;
    y.c = 6;
    struct s0 x;
    x = y;
    printf("%d\n", x.a);
    printf("%d\n", x.b);
    printf("%d\n", x.c);
}
