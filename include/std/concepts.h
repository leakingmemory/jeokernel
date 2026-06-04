//
// Created by sigsegv on 6/1/26.
//

#ifndef JEOKERNEL_CONCEPTS_H
#define JEOKERNEL_CONCEPTS_H

#include "type_traits.h"

namespace std {
    template <typename T, typename U> concept same_as = is_same<T, U>::value;
}

#endif //JEOKERNEL_CONCEPTS_H
