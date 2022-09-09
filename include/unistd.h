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

#endif //JEOKERNEL_UNISTD_H
