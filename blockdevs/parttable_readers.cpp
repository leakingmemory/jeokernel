//
// Created by sigsegv on 2/12/22.
//

#include <blockdevs/parttable_readers.h>
#include <blockdevs/mbr_reader.h>

parttable_readers::parttable_readers() : mbrReader(new mbr_reader()) {

}

std::shared_ptr<raw_parttable> parttable_readers::ReadParttable(std::shared_ptr<blockdev> blockdev) const {
    std::shared_ptr<raw_parttable> parttable{};
    parttable = mbrReader->ReadParttable(blockdev);
    return parttable;
}