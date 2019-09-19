#include <stdio.h>

int main() {
    struct ttt {
        char x;
        long y;
        int b[5];
    };
    int a[4] = {1, 3, 4, 5};
    int x = 9;
    printf("%d\n", sizeof(x = x + 5));
    printf("%d\n", x);
    printf("%d\n", (sizeof(struct ttt)
                    >= sizeof(char) + sizeof(long) + 5*sizeof(int)));
    printf("%d\n", sizeof a == sizeof *a * 4);
    printf("%d\n", sizeof(a) / sizeof(*a));
    printf("%d\n", sizeof(char) == 1);
    printf("%d\n", sizeof(char) == 2);
    printf("%d\n", sizeof(unsigned char) != 1);
    printf("%d\n", sizeof(signed char) != 1);
    printf("%d\n", 1 > sizeof(short));
    printf("%d\n", sizeof(short) == sizeof(unsigned short));
    printf("%d\n", sizeof(int) == sizeof(unsigned));
    printf("%d\n", sizeof(long) == sizeof(unsigned long));
    printf("%d\n", sizeof(short) <= sizeof(int));
    printf("%d\n", sizeof(int) <= sizeof(long));
}
