//
// Created by sigsegv on 6/28/21.
//

#ifndef JEOKERNEL_CTYPE_H
#define JEOKERNEL_CTYPE_H

#include <stdint.h>

int isalnum(int c);
int isalpha(int c);
int isdigit(int c);
int isxdigit(int c);
int isprint(int c);
int islower(int c);
int isupper(int c);
int isspace(int c);

int toupper(int c);
int tolower(int c);


#endif //JEOKERNEL_CTYPE_H
