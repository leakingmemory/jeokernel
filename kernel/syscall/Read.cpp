//
// Created by sigsegv on 9/14/22.
//

#include "Read.h"
#include <iostream>
#include <errno.h>

int64_t Read::Call(int64_t fd, int64_t uptr_buf, int64_t count, int64_t, SyscallAdditionalParams &params) {
    std::cout << "read(" << std::dec << fd << ", 0x" << std::hex << uptr_buf << std::dec << ", " << count << ")\n";
    return -EIO;
}