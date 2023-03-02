//
// Created by sigsegv on 7/7/21.
//

#ifndef JEOKERNEL_UNISTD_H
#define JEOKERNEL_UNISTD_H

#include <stdlib.h>

#define R_OK    4
#define W_OK    2
#define X_OK    1
#define F_OK    0

int isatty(int fd);

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2
#define SEEK_DATA   3
#define SEEK_HOLE   4

#endif //JEOKERNEL_UNISTD_H
