//
// Created by sigsegv on 6/28/21.
//

#ifndef JEOKERNEL_ERRNO_H
#define JEOKERNEL_ERRNO_H

extern int errno;

#define ENOENT 2
#define EINTR  4
#define EBADF   9
#define EFAULT  14 // Bad address
#define EINVAL  22 // Invalid argument
#define ENOSYS  38 // Not a syscall

#endif //JEOKERNEL_ERRNO_H
