//
// Created by sigsegv on 2/10/22.
//

#ifndef FSBITS_MBR_READER_H
#define FSBITS_MBR_READER_H

#include <blockdevs/parttable_reader.h>

class mbr_reader : public parttable_reader {
public:
    mbr_reader();
    std::shared_ptr<raw_parttable> ReadParttable(std::shared_ptr<blockdev> blockdev) const override;
    std::shared_ptr<raw_parttable> ReadParttable(std::shared_ptr<blockdev> blockdev, bool extendedTable) const;
};

#endif //FSBITS_MBR_READER_H
