#include <stdio.h>

int main() {
    int x = 10;
    int i;
    int a[3] = {1, 4, 9};
    int* p = a;
    double y = 9;
    ++y;
    ++y;
    --y;
    printf("%f\n", y);
    while (x --> 0)
    {
        printf("%d\n", ++x);
        printf("%d\n", --x);
        --x;
    }
    for (i = 0; i < 3; i++) {
        ++(*p);
        --(*p);
        ++(*p);
        printf("%d\n", *p++ + 13);
    }
}
