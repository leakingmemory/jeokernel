//
// Created by sigsegv on 9/22/22.
//

#include "ttyfsdev.h"
#include <sys/stat.h>

void ttyfsdev::stat(struct stat64 &st) const {
    st.st_mode |= S_IFCHR;
}

void ttyfsdev::stat(struct statx &st) const {
    st.stx_mode |= S_IFCHR;
}