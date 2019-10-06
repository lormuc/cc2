#define x    3
#define f(a) f(x * (a))
#undef  x
#define x    2
#define g    f
#define z    z[0]
#define m(a) a(w)
#define w    0,1
#define t(a) a

#define str(s)      # s
#define xstr(s)     str(s)
#define debug(s, t) printf("x" # s "= %d, x" # t "= %s", \
                           x ## s, x ## t)
#define INCFILE(n)  vers ## n
#define glue(a, b)  a ## b
#define xglue(a, b) glue(a, b)
#define HIGHLOW     "hello"
#define LOW         LOW ", world"

#include <stdio.h>

#define pr(x) printf("%s\n", xstr(x))

int main() {
    pr( f(y+1) + f(f(z)) % t(t(g)(0) + t)(1) );
    pr( g(x+(3,4)-w) | g(~ 5) & m (f)^m(m) );
    pr( debug(1, 2) );
    pr( ff(str(s("abc\0d", "abc", '\4') == 0) str(: @\n), s) );
    pr( glue(HIGH, LOW) );
    pr( xglue(HIGH, LOW) );
}
