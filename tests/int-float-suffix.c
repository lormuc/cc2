#include <stdio.h>

int main() {
    printf("%lu\n", sizeof(3.f));
    printf("%lu\n", sizeof(0.0));
    printf("%f\n", 1e308);
    printf("%f\n", 1.7e308);
    printf("%lu\n", sizeof(345u));
    printf("%lu\n", sizeof(1345ul));
    printf("%lu\n", sizeof(1l));
    printf("%lx\n", (1ul << 63));
    printf("%x\n", (1u << 31));
}
