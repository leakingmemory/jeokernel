//
// Created by sigsegv on 7/25/22.
//

#ifndef JEOKERNEL_UIO_H
#define JEOKERNEL_UIO_H

#include <sys/types.h>

struct iovec {
    void *iov_base;
    uintptr_t iov_len;
};

#endif //JEOKERNEL_UIO_H
