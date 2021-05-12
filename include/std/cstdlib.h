//
// Created by sigsegv on 5/12/21.
//

#ifndef JEOKERNEL_CSTDLIB_H
#define JEOKERNEL_CSTDLIB_H

template <class T> constexpr abs(T value) {
    return (value >= 0) ? value : (-value);
}

#endif //JEOKERNEL_CSTDLIB_H
