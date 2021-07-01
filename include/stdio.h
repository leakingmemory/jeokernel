//
// Created by sigsegv on 6/28/21.
//

#ifndef JEOKERNEL_STDIO_H
#define JEOKERNEL_STDIO_H
#ifdef __cplusplus

class FILE_abstract {
};

extern "C" {

#endif

typedef struct {
#ifdef __cplusplus
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

int printf(const char *format, ...);

#define fopen(...) ((FILE *) (void *) 0)

size_t fread(void *ptr, size_t size, size_t num, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t num, FILE *stream);
char *fgets(char *buf, int size, FILE *stream);
int fgetc(FILE *stream);
int feof(FILE *file);
int fflush(FILE *file);

#define ftell(stream) (-1)
#define fseek(...) (-1)

#define fclose(stream) (-1)


#ifdef __cplusplus
}
#endif

#endif //JEOKERNEL_STDIO_H
