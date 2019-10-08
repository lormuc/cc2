#  include     <stdio.h>

#    define a0( x   ) x+1
   #define a1(x) x+2

 # define a2(x) x+3
  #define ax a2(4)
  #   undef a2

int a2(int x) {
    return 99;
}

int main() {
    printf("%d\n", a0(0));
    printf("%d\n", a1(0));
    printf("%d\n", a2(0));
    printf("%d\n", ax);
}
