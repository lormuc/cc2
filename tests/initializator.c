#include <stdio.h>

int main() {
    int a = 8;
    printf("%d\n", a);
    float b = {9.0};
    printf("%f\n", b);
    struct c {
        int d;
        float e;
    };
    struct c f = {77, 66.0};
    struct c g = f;
    printf("%d\n", g.d);
    printf("%f\n", g.e);
    struct c t[3] = {{3, 5.0}, {4, 6.0}, {5, 7.0}};
    printf("%f\n", t[0].e + t[1].e + t[2].e);
}
