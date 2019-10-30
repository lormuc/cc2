#include <stdio.h>

// */
// /**/ abc
//\
part of a comment
/\
/ part of a comment

int main() {
    int x; // !@#$^&*()
    printf("a//b\n");
    /**//**/ printf("%d\n", 5/**//6);
    printf("%d\n", //**/ xyz
           42);
}
