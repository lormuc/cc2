#include <stdio.h>

int main() {
    printf("%c\n", '\'');
    printf("%c\n", '\"');
    printf("%c\n", '\\');
    printf("%s\n", "\\\"\'\"\\");
    printf("%c\n", '\x40');
    printf("%c\n", '\100');
    printf("%s\n", "\100000");
    printf("%s\n", "\x32""49");
}
