#ifndef __STDARG_H
#define __STDARG_H

typedef struct { int a; int b; char* c; char* d; } __builtin_va_list;

#ifndef _VA_LIST
typedef __builtin_va_list va_list;
#define _VA_LIST
#endif
/* #define va_start(ap, param) __builtin_va_start(ap, param) */
/* #define va_end(ap)  __builtin_va_end(ap) */
/* #define va_arg(ap, type)  __builtin_va_arg(ap, type) */

#ifndef __GNUC_VA_LIST
#define __GNUC_VA_LIST 1
typedef __builtin_va_list __gnuc_va_list;
#endif

#endif
