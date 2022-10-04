//
// Created by sigsegv on 9/22/22.
//

#include "ttyfsdev.h"
#include <sys/stat.h>

void ttyfsdev::stat(struct stat &st) {
    st.st_mode |= S_IFCHR;
}