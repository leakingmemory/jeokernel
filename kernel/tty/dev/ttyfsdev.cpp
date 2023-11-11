//
// Created by sigsegv on 9/22/22.
//

#include "ttyfsdev.h"
#include <sys/stat.h>

std::shared_ptr<ttyfsdev> ttyfsdev::Create(const std::shared_ptr<ttydev> &ttyd) {
    devfs_fsresourcelockfactory lockfactory{};
    std::shared_ptr<ttyfsdev> dev{new ttyfsdev(lockfactory, ttyd)};
    dev->SetSelfRef(dev);
    return dev;
}

void ttyfsdev::stat(struct stat64 &st) const {
    st.st_mode |= S_IFCHR;
}

void ttyfsdev::stat(struct statx &st) const {
    st.stx_mode |= S_IFCHR;
}