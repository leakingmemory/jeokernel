//
// Created by sigsegv on 7/1/21.
//

#ifndef JEOKERNEL_STAT_H
#define JEOKERNEL_STAT_H

#include <sys/types.h>

/*
 * Stub to compile acpica
 */

#ifdef __cplusplus
extern "C" {
#endif

    struct stat {
        dev_t st_dev;
        ino_t st_ino;
        mode_t st_mode;
        nlink_t st_nlink;
        uid_t st_uid;
        gid_t st_gid;
        dev_t st_rdev;
        off_t st_size;
        blksize_t st_blksize;
        blkcnt_t st_blocks;

        struct timespec st_atim;
        struct timespec st_mtim;
        struct timespec st_ctim;
    };

    #define S_IFIFO   0010000
    #define S_IFCHR   0020000
    #define S_IFDIR   0040000
    #define S_IFBLK   0060000
    #define S_IFREG  00100000
    #define S_IFLNK  00120000
    #define S_IFSOCK 00140000

    int stat(const char *pathname, struct stat *statbuf);

#ifdef __cplusplus
};
#endif

#endif //JEOKERNEL_STAT_H
