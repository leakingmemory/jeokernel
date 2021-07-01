//
// Created by sigsegv on 17.04.2021.
//

#ifndef JEOKERNEL_STDINT_H
#define JEOKERNEL_STDINT_H

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short signed int int16_t;
typedef short unsigned int uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef signed long long int int64_t;
typedef unsigned long long int uint64_t;

#ifdef __cplusplus
static_assert(sizeof(int8_t) == 1);
static_assert(sizeof(uint8_t) == 1);
static_assert(sizeof(int16_t) == 2);
static_assert(sizeof(uint16_t) == 2);
static_assert(sizeof(int32_t) == 4);
static_assert(sizeof(uint32_t) == 4);
static_assert(sizeof(int64_t) == 8);
static_assert(sizeof(uint64_t) == 8);
#endif

#ifdef IA32
typedef uint32_t size_t;
typedef int32_t ssize_t;
#else
typedef unsigned long int size_t;
typedef signed long int ssize_t;
#endif

#endif //JEOKERNEL_STDINT_H
