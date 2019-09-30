#include <stdio.h>

struct g { int y; }* f(struct a *n, struct a { int x; } *z)
{
    struct a y;
    struct g h;
    struct g { int z; };
    struct g u;
    y.x = 4;
    h.y = 3;
    u.z = 5;
    printf("%d\n", y.x);
    printf("%d\n", h.y);
    printf("%d\n", u.z);
    return 0;
}

void e() {
    struct g h;
    h.y = 5;
    printf("%d\n", h.y);
}

int main()
{
    struct g { int y; };
    struct c *cc = 0;
    {
        struct g a;
        a.y = 4;
        printf("%d\n", a.y);
    }
    printf("%d\n", cc == 0);
    printf("%d\n", f(0, 0) == 0);
}
