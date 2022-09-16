//
// Created by sigsegv on 2/9/22.
//

extern "C" {
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
}
#include <iostream>
#include <fileaccess/file_blockdev.h>

class file_blockdev_block : public blockdev_block {
private:
    void *ptr;
    std::size_t size;
public:
    file_blockdev_block(void *ptr, std::size_t size) : ptr(ptr), size(size) { }
    void *Pointer() const override;
    std::size_t Size() const override;
};

void *file_blockdev_block::Pointer() const {
    return ptr;
}

std::size_t file_blockdev_block::Size() const {
    return size;
}

file_blockdev::file_blockdev(const std::string &filename, std::size_t blocksize, uintptr_t sys_dev_id) : blocksize(blocksize), blocks(0), sys_dev_id(sys_dev_id), fd(-1) {
    fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0) {
        std::cerr << filename << ": " << strerror(errno) << "\n";
        return;
    }
    std::size_t length = lseek(fd, 0, SEEK_END);
    if (length < 0) {
        std::cerr << filename << ": lseek: " << strerror(errno) << "\n";
        close(fd);
        fd = -1;
        return;
    }
    blocks = length / blocksize;
    if ((length % blocksize) != 0) {
        ++blocks;
    }
}

file_blockdev::~file_blockdev() {
    if (fd >= 0) {
        close(fd);
    }
}

uintptr_t file_blockdev::GetDevId() const {
    return sys_dev_id;
}

std::size_t file_blockdev::GetBlocksize() const {
    return blocksize;
}

std::size_t file_blockdev::GetNumBlocks() const {
    return blocks;
}

std::shared_ptr<blockdev_block> file_blockdev::ReadBlock(size_t blocknum, size_t blocks) const {
    if ((blocknum + blocks) >= this->blocks) {
        if (blocknum >= blocks) {
            return {};
        }
        blocks = this->blocks - blocknum;
    }
    std::size_t offset{blocknum * blocksize};
    if (lseek(fd, offset, SEEK_SET) != offset) {
        return {};
    }
    size_t bytes{blocks * blocksize};
    void *ptr = malloc(bytes);
    if (ptr == nullptr) {
        std::cerr << strerror(errno) << "\n";
        return {};
    }
    size_t rd = read(fd, ptr, bytes);
    if (rd < 0) {
        free(ptr);
        return {};
    }
    if (rd < bytes) {
        std::cerr << "Short read " << rd << "<" << bytes << "\n";
        bzero(((char *) ptr) + rd, bytes - rd);
    }
    std::shared_ptr<blockdev_block> block{new file_blockdev_block(ptr, bytes)};
    return block;
}
