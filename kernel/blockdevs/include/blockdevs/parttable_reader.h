//
// Created by sigsegv on 2/10/22.
//

#ifndef FSBITS_PARTTABLE_READER_H
#define FSBITS_PARTTABLE_READER_H

#include <blockdevs/blockdev.h>
#include <blockdevs/raw_parttable.h>
#include <memory>

class parttable_reader {
public:
    virtual ~parttable_reader() { }
    virtual std::shared_ptr<raw_parttable> ReadParttable(std::shared_ptr<blockdev> blockdev) const = 0;
};

#endif //FSBITS_PARTTABLE_READER_H
