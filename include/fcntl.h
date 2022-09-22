//
// Created by sigsegv on 8/10/22.
//

#ifndef JEOKERNEL_FCNTL_H
#define JEOKERNEL_FCNTL_H

#define AT_FDCWD            -100
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR        0x200
#define AT_SYMLINK_FOLLOW   0x400
#define AT_NO_AUTOMOUNT     0x800
#define AT_EMPTY_PATH       0x1000

#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2

#define O_NONBLOCK          0x800
#define O_CLOEXEC           0x80000

#endif //JEOKERNEL_FCNTL_H
