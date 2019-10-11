#include <stdio.h>

typedef int int_;

typedef int* int_p;

typedef struct {
    int_ a;
    int_ b;
} s0;

typedef int arr_[2][3];

int_ ff(int_ (int_));

int_ h(int_ x) {
    printf("%d\n", x);
    return 0;
}

int main() {
    int_ x;
    int_p y;
    y = &x;
    arr_ ar;
    s0 s;
    ar[1][2] = 3;
    x = 43;
    printf("%d\n", *y);
    printf("%d\n", ar[1][2]);
    s.a = 3;
    s.b = 9;
    printf("%d\n", s.a);
    printf("%d\n", s.b);
    {
        int int_;
        int_ = 934;
        printf("%d\n", int_ + 1);
        {
            typedef struct { int y; } int_;
            int_ z;
            z.y = 34;
            printf("%d\n", z.y);
        }
        printf("%d\n", int_);
    }
}

int_ ff(int_ g(int_)) {
    g(93);
    return 0;
}
