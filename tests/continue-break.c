#include <stdio.h>

int main() {
    int i = 0;
    int j = 0;
    while (i < 10) {
        int k;
        j = j + 2;
        if (i < 5) {
            i = i + 1;
            continue;
        }
        i = i + 1;
        k = 0;
        while (1) {
            if (k < 4) {
            } else {
                break;
            }
            k = k + 1;
            continue;
        }
    }
    printf("%d\n", j);
    return 0;
}
