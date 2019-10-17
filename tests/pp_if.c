#include <stdio.h>

#define abc
#define ghi 3049 3548
#define one 1
#define two 2

#define pr(y) printf("%d\n", y)

int main() {
#if a0
    pr(98);
#endif
#if (one && two) || a1
    pr(98);
#endif
#define pr(y) printf("%d\n", y)
#if defined(abc) && defined(def)
    pr(0);
#if 0
    pr(1);
#endif
#endif

#if defined abc
#if 1 + 1 == 2
    pr(2);
#endif
#if 3 > 20
    pr(3);
#if defined ghi
    pr(4);
#endif
#endif
#endif

#if !defined ghi || defined(abc)
    pr(5);
#endif

#if one < two
    pr(6);
#endif
#if one > two
    pr(7);
#endif
}
