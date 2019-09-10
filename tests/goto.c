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
        int z;
        {
            int z = 4;
            {
                int y = 0;
            l0:
                if (1) y = 9;
                printf("%d\n", y);
            l1:
                z = 1;
                goto l2;
            }
        }
        while (1 <= z) {
        l2:
            z = z - 1;
        }
        goto l3;
    }
l3:
    j = j + 1;
    if (j < 3) goto loop;
    return 0;
}
