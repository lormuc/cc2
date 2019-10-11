#include <stdio.h>

struct s0 {
    int x;
};

struct s1 {
    struct s0 * p;
    int y;
};

int main() {
    struct s0 a;
    struct s1 b;
    struct s1 * p;
    b.p = &a;
    b.y = 3;
    p = &b;
    p->y = 9;
    p->p->x = 34;
    printf("%d\n", p->y);
    printf("%d\n", p->p->x);
}
