//
// Created by sigsegv on 6/28/21.
//

#ifndef JEOKERNEL_STDLIB_H
#define JEOKERNEL_STDLIB_H

#include <core/malloc.h>

#ifndef NULL
#ifdef __cplusplus
#define NULL nullptr
#else
#define NULL ((void *) 0)
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif
unsigned long strtoul(const char *nptr, char **endptr, int base);
long strtol(const char *nptr, char **endptr, int base);

void exit(int code);
void abort();
#ifdef __cplusplus
}
#endif

#endif //JEOKERNEL_STDLIB_H
