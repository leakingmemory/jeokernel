//
// Created by sigsegv on 7/29/22.
//

#include <core/uname_info.h>

uname_info get_uname_info() {
    return {
        .linux_level = "5.10.0",
        .variant = "jeokernel",
        .arch = "x86_64"
    };
}