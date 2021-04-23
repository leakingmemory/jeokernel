//
// Created by sigsegv on 17.04.2021.
//

#ifndef JEOKERNEL_STDINT_H
#define JEOKERNEL_STDINT_H

typedef signed char int8_t;
static_assert(sizeof(int8_t) == 1);
typedef unsigned char uint8_t;
static_assert(sizeof(uint8_t) == 1);
typedef short signed int int16_t;
static_assert(sizeof(int16_t) == 2);
typedef short unsigned int uint16_t;
static_assert(sizeof(uint16_t) == 2);
typedef signed int int32_t;
static_assert(sizeof(int32_t) == 4);
typedef unsigned int uint32_t;
static_assert(sizeof(uint32_t) == 4);
typedef signed long long int int64_t;
static_assert(sizeof(int64_t) == 8);
typedef unsigned long long int uint64_t;
static_assert(sizeof(uint64_t) == 8);

#ifdef IA32
typedef uint32_t size_t;
#else
typedef unsigned long int size_t;
#endif

#endif //JEOKERNEL_STDINT_H
