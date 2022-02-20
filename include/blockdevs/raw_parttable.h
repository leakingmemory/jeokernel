//
// Created by sigsegv on 2/10/22.
//

#ifndef FSBITS_RAW_PARTTABLE_H
#define FSBITS_RAW_PARTTABLE_H

#include <cstdint>
#include <blockdevs/parttable_entry.h>
#include <memory>
#include <vector>

class raw_parttable {
public:
    virtual ~raw_parttable() { }
    virtual uint64_t GetSignature() = 0;
    virtual std::size_t GetBlockSize() = 0;
    virtual std::vector<std::shared_ptr<parttable_entry>> GetEntries() = 0;
};

#endif //FSBITS_RAW_PARTTABLE_H
