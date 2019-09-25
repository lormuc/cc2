#include <stdio.h>

int main() {
    int a[3] = {'a', 'b', 'c'};
    int i;
    for (i = 0; i < 10; i++) {
        printf("%c", '0' + i);
    }
    printf("\n");
    printf("%c\n", a[1]);
    printf("%d\n", sizeof('0') == sizeof(int));
}
