#include <stdio.h>

void f(int c(int)) {
    c(1);
}

void h(int a[4]) {
    printf("%lu\n", sizeof(a));
    return;
    return;
}

int g(int e) {
    printf("%d\n", e);
    return e;
}

int main()
{
    int a[3];
    h(a);
    f(g);
}
