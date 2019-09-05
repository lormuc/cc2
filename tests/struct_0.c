#include <stdio.h>

int main() {
    struct s2 { float tt; };
    struct s0 {
        int x;
        float y;
        int z;
        struct {
            int x;
            int y;
            struct s1 {
                int a;
                float b;
            } z;
        } w;
        struct s2 vv;
    } xx;
    struct s0 yy;
    xx.w.z.b = 4.5;
    xx.vv.tt = 9.5;
    yy.x = 9;
    printf("%f\n", xx.w.z.b);
    printf("%d\n", yy.x);
    printf("%f9999\n", xx.vv.tt);
    {
        struct s0 {
            int zzz;
        };
        struct s0 www;
        www.zzz = 99;
        printf("%d\n", www.zzz);
    }
    return 0;
}
