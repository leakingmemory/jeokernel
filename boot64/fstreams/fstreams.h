//
// Created by sigsegv on 7/6/21.
//

#ifndef JEOKERNEL_FSTREAMS_H
#define JEOKERNEL_FSTREAMS_H

#include <stdio.h>
#include <stdint.h>

class FILE_impl : public FILE_abstract {
private:
    FILE file;
public:
    FILE_impl();
    virtual ~FILE_impl() noexcept;
    FILE *File() {
        return &file;
    }
    FILE_impl *Impl();
    virtual char *gets(char *buf, size_t size) = 0;
    virtual int getchar() = 0;
    virtual void ungetchar(int c) = 0;
    virtual size_t read(void *ptr, size_t size, size_t nmemb) = 0;
    virtual int flush() = 0;
    virtual bool eof() = 0;
    virtual bool error() = 0;
    virtual void clearerr() = 0;
    virtual int fileno() = 0;
};

void init_fstreams();
void init_stdout();
void init_stdin();

#endif //JEOKERNEL_FSTREAMS_H
