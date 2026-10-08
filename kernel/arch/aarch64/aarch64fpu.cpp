//
// Created by sigsegv on 3/2/23.
//

#include <core/aarch64fpu.h>

static aarch64_fpu_state fpu0_frame;

const aarch64_fpu_state &get_fpu0_initial() {
    return fpu0_frame;
}
