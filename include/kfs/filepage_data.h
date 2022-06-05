//
// Created by sigsegv on 6/5/22.
//

#ifndef JEOKERNEL_FILEPAGE_DATA_H
#define JEOKERNEL_FILEPAGE_DATA_H

#include <cstdint>

class filepage_data {
    friend filepage;
private:
    uintptr_t physpage;
public:
    filepage_data();
    ~filepage_data();
    filepage_data(const filepage_data &) = delete;
    filepage_data(filepage_data &&) = delete;
    filepage_data &operator =(const filepage_data &) = delete;
    filepage_data &operator =(filepage_data &&) = delete;
};

#endif //JEOKERNEL_FILEPAGE_DATA_H
