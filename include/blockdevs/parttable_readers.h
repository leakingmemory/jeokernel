//
// Created by sigsegv on 2/12/22.
//

#ifndef FSBITS_PARTTABLE_READERS_H
#define FSBITS_PARTTABLE_READERS_H

#include <blockdevs/parttable_reader.h>

class mbr_reader;
class gpt_reader;

class parttable_readers : public parttable_reader {
private:
    std::shared_ptr<mbr_reader> mbrReader;
    std::shared_ptr<gpt_reader> gptReader;
public:
    parttable_readers();
    std::shared_ptr<raw_parttable> ReadParttable(std::shared_ptr<blockdev> blockdev) const;
};

#endif //FSBITS_PARTTABLE_READERS_H
