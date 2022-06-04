//
// Created by sigsegv on 4/23/22.
//

#ifndef FSBITS_FILEITEM_H
#define FSBITS_FILEITEM_H

#include <cstdint>

class fileitem {
public:
    virtual ~fileitem() = default;
    virtual uint32_t Mode() = 0;
    virtual std::size_t Size() = 0;
};

#endif //FSBITS_FILEITEM_H
