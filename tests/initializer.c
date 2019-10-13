#include <stdio.h>

int main() {
    int a = 8;
    printf("%d\n", a);
    float b = {9.0};
    printf("%f\n", b);
    struct c {
        int d;
        float e;
    };
    struct c f = {77, 66.0};
    struct c g = f;
    printf("%d\n", g.d);
    printf("%f\n", g.e);
    struct c t[3] = {{3, 5.0}, {4, 6.0}, {5, 7.0}};
    printf("%f\n", t[0].e + t[1].e + t[2].e);
    float y[4][3] = {
        { 1, 3, 5 },
        { 2, 4, 6 },
        { 3, 5, 7 },
    };
    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 3; j++) {
            printf(" %f", y[i][j]);
        }
        printf("\n");
    }
    printf("\n");
    float z[4][3] = {
        1, 3, 5, 2, 4, 6, 3, 5, 7
    };
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 3; j++) {
            printf(" %f", z[i][j]);
        }
        printf("\n");
    }
    printf("\n");
    float zz[4][3] = {
        { 1 }, { 2 }, { 3 }, { 4 }
    };
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 3; j++) {
            printf(" %f", zz[i][j]);
        }
        printf("\n");
    }
    printf("\n");
    short q[4][3][2] = {
        { 1 },
        { 2, 3 },
        { 4, 5, 6 }
    };
    int k;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 3; j++) {
            for (k = 0; k < 2; k++) {
                printf(" %d", q[i][j][k]);
            }
        }
        printf("\n");
    }
    printf("\n");
    {
        short q[4][3][2] = {
            1, 0, 0, 0, 0, 0,
            2, 3, 0, 0, 0, 0,
            4, 5, 6
        };
        int i, j, k;
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 3; j++) {
                for (k = 0; k < 2; k++) {
                    printf(" %d", q[i][j][k]);
                }
            }
            printf("\n");
        }
        printf("\n");
    }
    {
        short q[4][3][2] = {
            {
                { 1 },
            },
            {
                { 2, 3 },
            },
            {
                { 4, 5 },
                { 6 },
            }
        };
        int i, j, k;
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 3; j++) {
                for (k = 0; k < 2; k++) {
                    printf(" %d", q[i][j][k]);
                }
            }
            printf("\n");
        }
        printf("\n");
    }
    {
        int x[] = { 1, 3, 5 };
        int i;
        for (i = 0; i < 3; i++) {
            printf("%d ", x[i]);
        }
        printf("\n");
    }
    {
        struct { int a[3], b; } w[] = { { 1 }, 2, 3, 4 };
        printf("%d\n", w[0].a[0]);
        printf("%d\n", w[0].a[1]);
        printf("%d\n", w[0].a[2]);
        printf("%d\n", w[0].b);
        printf("%d\n", w[1].a[0]);
        printf("%d\n", w[1].a[1]);
        printf("%d\n", w[1].a[2]);
        printf("%d\n", w[1].b);
    }
    {
        char s[] = "abc", t[3] = {"abc"};
        printf("%ld\n", sizeof(s));
        printf("%ld\n", sizeof(t));
        printf("%s\n", s);
        printf("%c%c%c\n", t[0], t[1], t[2]);
    }
    /* { */
    /*     char* ss[] = {"aaa", "bbb", "ccc"}; */
    /*     int i; */
    /*     for (i = 0; i < 3; i++) { */
    /*         printf("%s\n", ss[i]); */
    /*     } */
    /* } */
}
