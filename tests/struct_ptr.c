#include <stdio.h>

int main() {
    struct tt { int a; int b; };
    struct tt yy;
    struct tt* xx;
    struct tt** zz;
    xx = &yy;
    zz = &xx;
    yy.a = 9;
    (*xx).b = 18;
    (**zz).b = (*xx).b + 10;
    printf("%d\n", (*xx).a + (*xx).b);
}
