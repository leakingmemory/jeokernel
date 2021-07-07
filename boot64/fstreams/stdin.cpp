//
// Created by sigsegv on 7/6/21.
//

#include "fstreams.h"

class Stdin_impl : public FILE_impl {
public:
    Stdin_impl() : FILE_impl() {
    }
    ~Stdin_impl() {
    }
    virtual char *gets(char *buf, size_t size) {
        return (char *) 0;
    }
    virtual int getchar() {
        return EOF;
    }
    virtual void ungetchar(int c) {
    }
    virtual size_t read(void *ptr, size_t size, size_t nmemb) {
        return -1;
    }
    virtual int flush() {
        return 0;
    }
    virtual bool eof() {
        return true;
    }
    virtual bool error() {
        return false;
    }
    virtual void clearerr() {
        ;
    }
    virtual int fileno() {
        return 0;
    }
};

extern "C" {
    FILE *stdin = nullptr;
}

void init_stdin() {
    stdin = (new Stdin_impl())->File();
}