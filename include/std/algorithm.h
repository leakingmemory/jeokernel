//
// Created by sigsegv on 11/4/23.
//

#ifndef JEOKERNEL_STD_ALGORITHM_H
#define JEOKERNEL_STD_ALGORITHM_H

namespace std {
    template<class Iterator, class VType> Iterator find(Iterator start, Iterator end, VType value) {
        Iterator iterator = start;
        while (iterator != end) {
            if (*iterator == value) {
                return iterator;
            }
            ++iterator;
        }
        return end;
    }
};

#endif //JEOKERNEL_STD_ALGORITHM_H
