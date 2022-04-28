//
// Created by sigsegv on 4/28/22.
//

#ifndef JEOKERNEL_ASSERT_H
#define JEOKERNEL_ASSERT_H

#include <klogger.h>

#define assert(assertion) { \
if (!(assertion)) {         \
wild_panic("assertion failed"); \
}                           \
}

#endif //JEOKERNEL_ASSERT_H
