//
// Created by sigsegv on 7/6/21.
//

#include "fstreams.h"

FILE_impl::FILE_impl() : file() {
    file.impl = this;
}

FILE_impl::~FILE_impl() noexcept {
}

FILE_impl * FILE_impl::Impl() {
    return this;
}

extern "C" {

    char *fgets(char *buf, int size, FILE *stream) {
        return stream->impl->Impl()->gets(buf, size);
    }

    int fgetc(FILE *stream) {
        return stream->impl->Impl()->getchar();
    }

    size_t fread(void *ptr, size_t size, size_t num, FILE *stream) {
        return stream->impl->Impl()->read(ptr, size, num);
    }

    int fflush(FILE *stream) {
        return stream->impl->Impl()->flush();
    }

    int feof(FILE *stream) {
        return stream->impl->Impl()->eof() ? 1 : 0;
    }

    int ferror(FILE *stream) {
        return stream->impl->Impl()->error() ? 1 : 0;
    }

    void clearerr(FILE *stream) {
        stream->impl->Impl()->clearerr();
    }

    int fileno(FILE *stream) {
        return stream->impl->Impl()->fileno();
    }

    int ungetc(int c, FILE *stream) {
        stream->impl->Impl()->ungetchar(c);
        return c;
    }
};

void init_fstreams() {
    init_stdout();
    init_stdin();
}