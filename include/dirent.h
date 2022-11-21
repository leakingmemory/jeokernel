//
// Created by sigsegv on 11/18/22.
//

#ifndef JEOKERNEL_DIRENT_H
#define JEOKERNEL_DIRENT_H

#include <cstdint>

struct dirent64 {
    uint64_t d_ino;
    int64_t d_off;
    unsigned short int d_reclen;
    unsigned char d_type;
    char d_name[256];

#ifdef __cplusplus
    constexpr static std::size_t Reclen(const std::string &name) {
        return offsetof(dirent64, d_name) + name.size() + 1;
    }
#endif
};

#define DT_DIR  4
#define DT_REG  8

#endif //JEOKERNEL_DIRENT_H
