#include <stdio.h>

int f() {
    static int i;
    {
        static int i;
        i += 2;
        printf("%d\n", i);
    }
    i++;
    return i;
}

int main()
{
    static struct { int e; } x;
    x.e = 5;
    printf("%d\n", x.e);
    printf("%d\n", f());
    printf("%d\n", f());
    printf("%d\n", f());
}
