#include <stdio.h>

union uu {
    int x;
    long y;
} xx;
int* p = &xx.x;

int main() {
    struct ss {
        int a;
        int b;
    };
    union uu { int a; struct ss b; };
    union uu x;
    union uu* f = &x;
	union { int a; int b; } v;
    x.a = 9;
    f->a++;
    x.b.a = 10;
    printf("%d\n", x.a);
    printf("%d\n", x.b.a);
    *p = 99;
    printf("%d\n", *p);
    v.a = 1;
    v.b = 3;
    printf("%d\n", (v.a != 3 || v.b != 3));
}
