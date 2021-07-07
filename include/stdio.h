//
// Created by sigsegv on 6/28/21.
//

#ifndef JEOKERNEL_STDIO_H
#define JEOKERNEL_STDIO_H

#include <stdarg.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>

#ifdef __cplusplus

class FILE_impl;

class FILE_abstract {
public:
    virtual ~FILE_abstract() {}
    virtual FILE_impl *Impl() = 0;
};

#define HAS_FILE_abstract

extern "C" {

#endif

typedef struct {
#ifdef HAS_FILE_abstract
    FILE_abstract *impl;
#endif
} FILE;

void putchar(char ch);
int getchar();

#define EOF 0x1000

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int sprintf(char *str, const char *fmt, ...);

int vfprintf(FILE *file, const char *fmt, va_list args);

int
fprintf (
        FILE                    *File,
        const char              *Format,
        ...);

int vprintf(const char *format, va_list args);
int printf(const char *format, ...);

#define fopen(...) ((FILE *) (void *) 0)
#define freopen(...) ((FILE *) (void *) 0)

size_t fread(void *ptr, size_t size, size_t num, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t num, FILE *stream);
char *fgets(char *buf, int size, FILE *stream);
int fgetc(FILE *stream);
#define getc fgetc
int feof(FILE *file);
int ferror(FILE *file);
void clearerr(FILE *file);
int fflush(FILE *file);
int fileno(FILE *file);

#define ftell(stream) (-1)
#define fseek(...) (-1)

#define fclose(stream) (-1)

#define remove(pathname) (-1)

void perror(const char *s);

#ifdef __cplusplus
};
#endif

#endif //JEOKERNEL_STDIO_H
