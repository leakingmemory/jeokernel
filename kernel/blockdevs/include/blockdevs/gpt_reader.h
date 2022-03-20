//
// Created by sigsegv on 2/20/22.
//

#ifndef FSBITS_GPT_READER_H
#define FSBITS_GPT_READER_H

#include <blockdevs/parttable_reader.h>

class gpt_reader : public parttable_reader {
public:
    gpt_reader();
    std::shared_ptr<raw_parttable> ReadParttable(std::shared_ptr<blockdev> blockdev) const override;
};

#endif //FSBITS_GPT_READER_H
