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
    struct c f;
    struct c t[3] = {{3, 5.0}, {4, 6.0}, {5, 7.0}};
    printf("%f\n", t[0].e + t[1].e + t[2].e);
}
