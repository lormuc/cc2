#include <stdio.h>

int main() {
    int a[3] = {1, 2, 3};
    int* p = a;
    printf("%d\n", *p++);
    printf("%d\n", *p);
    p++;
    printf("%d\n", *p--);
    printf("%d\n", *p);
    p--;
    printf("%d\n", *p);
}
