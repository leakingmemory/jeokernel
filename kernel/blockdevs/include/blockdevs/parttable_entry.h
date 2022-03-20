//
// Created by sigsegv on 2/10/22.
//

#ifndef FSBITS_PARTTABLE_ENTRY_H
#define FSBITS_PARTTABLE_ENTRY_H

#include <cstdint>

class parttable_entry {
public:
    virtual ~parttable_entry() { }
    virtual std::size_t GetOffset() = 0;
    virtual std::size_t GetSize() = 0;
    virtual unsigned int GetType() = 0;
};

#endif //FSBITS_PARTTABLE_ENTRY_H
