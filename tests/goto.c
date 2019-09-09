#include <stdio.h>

int main() {
    int i = 0;
    int j = 0;
loop:
    i = i + 1;
    if (i < 5) goto loop;
    printf("%d\n", i);
    goto l0;
    {
        {
            int z = 4;
            {
                int y = 0;
            l0:
                y = 9;
                printf("%d\n", y);
            l1:
                goto l2;
            }
        }
    l2:
        goto l3;
    }
l3:
    j = j + 1;
    if (j < 3) goto loop;
    return 0;
}
