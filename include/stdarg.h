//
// Created by sigsegv on 6/20/21.
//

#ifndef JEOKERNEL_STDARG_H
#define JEOKERNEL_STDARG_H

typedef __builtin_va_list va_list;

#define va_start(lst, param) __builtin_va_start(lst, param)
#define va_arg(lst, typ) __builtin_va_arg(lst, typ)
#define va_end(lst) __builtin_va_end(lst)

#endif //JEOKERNEL_STDARG_H
