//
// Created by sigsegv on 26.04.2021.
//

#ifndef JEOKERNEL_CSTDINT_H
#define JEOKERNEL_CSTDINT_H

#include <stdint.h>

#ifdef __cplusplus
namespace std {
    typedef int64_t intmax_t;
    typedef uint64_t uintmax_t;

    typedef size_t size_t;
    typedef ssize_t ssize_t;
    typedef void *pointer;
}
#endif

#endif //JEOKERNEL_CSTDINT_H
