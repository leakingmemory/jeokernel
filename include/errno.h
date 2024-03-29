//
// Created by sigsegv on 6/28/21.
//

#ifndef JEOKERNEL_ERRNO_H
#define JEOKERNEL_ERRNO_H

#ifdef __cplusplus
extern "C" {
#endif
extern int errno;
#ifdef __cplusplus
};
#endif

#define EPERM       1
#define ENOENT      2
#define ESRCH       3
#define EINTR       4
#define EIO         5
#define EBADF       9
#define EAGAIN      11
#define ENOMEM      12
#define EACCES      13
#define EFAULT      14 // Bad address
#define EBUSY       16
#define EEXIST      17
#define ENOTDIR     20
#define EISDIR      21
#define EINVAL      22 // Invalid argument
#define EMFILE      24
#define ENOSPC      28
#define ERANGE      34
#define ENOSYS      38 // Not a syscall
#define ELOOP       40
#define EOPNOTSUPP  95

#endif //JEOKERNEL_ERRNO_H
