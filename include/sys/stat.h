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

#ifdef __cplusplus
struct stat64 {
#else
struct stat {
#endif
    dev_t st_dev;
    ino_t st_ino;
    nlink_t st_nlink;
    mode_t st_mode;
    uid_t st_uid;
    gid_t st_gid;
    int explicit_padding;
    dev_t st_rdev;
    off_t st_size;
    blksize_t st_blksize;
    blkcnt_t st_blocks;

    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
};

#ifdef __cplusplus
    struct stat32 {
        dev_t st_dev;
        ino_t st_ino;
        mode_t st_mode;
        nlink_t st_nlink;
        uid_t st_uid;
        gid_t st_gid;
        dev_t st_rdev;
        short int explicit_padding;
        off_t st_size;
        blksize_t st_blksize;
        blkcnt_t st_blocks;

        struct timespec st_atim;
        struct timespec st_mtim;
        struct timespec st_ctim;
    };

    struct stat : public stat64 {
    };
#endif

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

struct statx_timestamp {
    int64_t tv_sec;
    uint32_t tv_nsec;
    uint32_t padding;
};

struct statx {
    uint32_t stx_mask;

    uint32_t stx_blksize;
    uint64_t stx_attributes;
    uint32_t stx_nlink;
    uint32_t stx_uid;
    uint32_t stx_gid;
    uint16_t stx_mode;
    uint16_t padding;
    uint64_t stx_ino;
    uint64_t stx_size;
    uint64_t stx_blocks;

    uint64_t stx_attributes_mask;

    struct statx_timestamp stx_atime;
    struct statx_timestamp stx_btime;
    struct statx_timestamp stx_ctime;
    struct statx_timestamp stx_mtime;

    uint32_t stx_rdev_major;
    uint32_t stx_rdev_minor;

    uint32_t stx_dev_major;
    uint32_t stx_dev_minor;
    uint64_t stx_mnt_id;
#ifndef KERNEL
    uint64_t end_padding[13];
#endif
};

#endif //JEOKERNEL_STAT_H
