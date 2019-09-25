#include <stdio.h>

int main() {
    int a = 3;
    int i;
    switch (a) {
    case 1: a++;
    case 2: a++;
    case 3: a++;
    case 4: a++; break;
    case 5: a++;
    }
    printf("%d\n", a);
    switch (3) {
    case 4:;
    case 5:;
    case 3: a++; break;
    case 6: a++;
    default: a++;
    }
    printf("%d\n", a);
    switch (7) {
    case 4:;
    case 5:;
    case 3: a++; break;
    case 6: a++;
    default: a = 100; break;
    }
    printf("%d\n", a);
    switch (7) {
    case 4:;
    case 7:
        switch (5) {
        case 4:;
        case 5:
            switch (4) {
            case 4:
                ;
            default:
                a++;
                break;
            }
            a++;
            break;
        case 7:;
        default:;
        }
        break;
    case 3: a++; break;
    case 6: a++;
    default: a = 100;
    }
    switch (8) {
    default:
        i = 0;
        while (1) {
            if (i == 4) {
                break;
            }
            i++;
        }
        a++;
    }
    printf("%d\n", a);
}
