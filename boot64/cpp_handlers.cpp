//
// Created by sigsegv on 19.04.2021.
//

#include <stdint.h>
#include <strings.h>
#include <string.h>
#include <core/malloc.h>

/*
 * This is just stubs for the compiler libraries to link. Currently the code
 * has nowhere to go if a failure occurs, so hlt-loops will just for end-user
 * look like a freeze.
 */

extern "C" {
    void __stack_chk_fail() {
        while (1) {
            asm("hlt");
        }
    }

    void abort() {
        while (1) {
            asm("hlt");
        }
    }

    int fwrite() {
        return 0;
    }
    int fputc() {
        return 0;
    }
    int fputs() {
        return 0;
    }

    void *stderr = nullptr;

    int write() {
        return 0;
    }

    int __sprintf_chk(char * str, int flag, uint32_t maxstrlen, const char * format) {
        size_t len{strlen(format)};
        size_t ret_len{len};
        if (len >= maxstrlen) {
            len = maxstrlen - 1;
        }
        bcopy(format, str, len);
        str[len] = 0;
        return ret_len;
    }

    int dl_iterate_phdr() {
        return 0;
    }

#ifndef CLANG
    void *operator new (size_t size)
    {
    return malloc (size);
    }

    void *operator new [] (size_t size)
    {
    return malloc (size);
    }

    void operator delete (void *p)
    {
        if (p) free (p);
    }

    void operator delete [] (void *p)
    {
        if (p) free (p);
    }

    void operator delete (void *p, size_t)
    {
        if (p) free (p);
    }
#else

    void *operator new(size_t size, void*ref) {
        return ref;
    }

#endif
}
