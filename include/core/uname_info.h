//
// Created by sigsegv on 7/29/22.
//

#ifndef JEOKERNEL_UNAME_INFO_H
#define JEOKERNEL_UNAME_INFO_H

#include <string>

struct uname_info {
    std::string linux_level;
    std::string variant;
    std::string arch;
};

uname_info get_uname_info();

#endif //JEOKERNEL_UNAME_INFO_H
