//
// Created by sigsegv on 3/1/23.
//

#include "Lseek.h"
#include <exec/fdesc.h>
#include <exec/procthread.h>
#include <unistd.h>
#include <errno.h>
#include "SyscallCtx.h"
#include <iostream>

//#define LSEEK_DEBUG

int64_t Lseek::Call(int64_t i_fd, int64_t offset, int64_t i_whence, int64_t, SyscallAdditionalParams &params) {
    SeekWhence whence;
    {
        unsigned int uint_whence = (unsigned int) i_whence;
        switch (uint_whence) {
            case SEEK_CUR:
                whence = SeekWhence::CUR;
                break;
            case SEEK_SET:
                whence = SeekWhence::SET;
                break;
            case SEEK_END:
                whence = SeekWhence::END;
                break;
            default:
                return -EINVAL;
        }
    }
    int fd = (int) i_fd;
    SyscallCtx ctx{params};
    auto fdesc = ctx.GetProcess().get_file_descriptor(fd);
    if (!fdesc) {
        return -EBADF;
    }
#ifdef LSEEK_DEBUG
    std::cout << "lseek(" << std::dec << fd << ", " << offset << ", ";
    switch (whence) {
        case SeekWhence::SET:
            std::cout << "SET";
            break;
        case SeekWhence::CUR:
            std::cout << "CUR";
            break;
        case SeekWhence::END:
            std::cout << "END";
            break;
    }
    std::cout << ") -> ";
#endif
    auto result = fdesc->seek(offset, whence);
#ifdef LSEEK_DEBUG
    std::cout << std::dec << result << "\n";
#endif
    return result;
}
