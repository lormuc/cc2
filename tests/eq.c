#include <stdio.h>

int main() {
    int a[3] = {3, 4, 5};
    void* p = 0;
    printf("%d\n", &a[1] == a + 1);
    printf("%d\n", a == 0);
    printf("%d\n", &a[1] + 1 == &a[2]);
    printf("%d\n", p == 0);
    printf("%d\n", 4 == 0);
    printf("%d\n", 4 != 0);
    printf("%d\n", 4.0 == 4.0);
    printf("%d\n", 4.0 != 4.0);
    printf("%d\n", -13 == -13);
    printf("%d\n", -13 != -13);
}
