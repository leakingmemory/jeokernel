//
// Created by sigsegv on 6/28/21.
//

#ifndef JEOKERNEL_STDLIB_H
#define JEOKERNEL_STDLIB_H

#include <core/malloc.h>

#ifndef NULL
#define NULL ((void *) 0)
#endif

unsigned long strtoul(const char *nptr, char **endptr, int base);

void exit(int code);

#endif //JEOKERNEL_STDLIB_H
