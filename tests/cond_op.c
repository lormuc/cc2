#include <stdio.h>

int main() {
#if (4 > 5) ? 0 : 0 ? 0 : 5
    printf("0\n");
#endif
    printf("%f\n", (9 && 8) ? 4 : 5.0);
    1 ? (void)(printf("1\n")) : (void)1;
    struct sss { int x; int y; };
    struct sss x0 = { 5, 6 };
    struct sss x1 = { 7, 8 };
    struct sss x2 = 1 ? x0 : x1;
    printf("%d %d\n", x2.x, x2.y);
}
