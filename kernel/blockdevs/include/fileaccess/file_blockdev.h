//
// Created by sigsegv on 2/9/22.
//

#ifndef FSBITS_FILE_BLOCKDEV_H
#define FSBITS_FILE_BLOCKDEV_H

#include <string>
#include <blockdevs/blockdev.h>

class file_blockdev : public blockdev {
private:
    std::size_t blocksize;
    std::size_t blocks;
    int fd;
public:
    file_blockdev(const std::string &filename, std::size_t blocksize);
    ~file_blockdev();
    std::size_t GetBlocksize() const override;
    std::size_t GetNumBlocks() const override;
    std::shared_ptr<blockdev_block> ReadBlock(size_t blocknum, size_t blocks) const override;
};


#endif //FSBITS_FILE_BLOCKDEV_H
