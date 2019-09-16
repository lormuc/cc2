#include <stdio.h>

int main() {
    int x = 4309;
    printf("%f\n", (double) 17 / 5);
    printf("%u\n", (unsigned int)(unsigned char) 256);
    (void)printf("%u\n", (unsigned int)(unsigned char) 259);
    printf("%u\n", (unsigned) (float) 4.5);
    printf("%d\n", *(int*)(void*)&x);
}
