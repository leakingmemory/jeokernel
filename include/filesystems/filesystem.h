//
// Created by sigsegv on 4/25/22.
//

#ifndef FSBITS_FILESYSTEM_H
#define FSBITS_FILESYSTEM_H

#include <memory>
#include <vector>

class blockdev;

class filesystem {
};

class blockdev_filesystem : public filesystem {
private:
    std::shared_ptr<blockdev> bdev;
public:
    blockdev_filesystem(std::shared_ptr<blockdev> bdev) : bdev(bdev) {
    }
};

class filesystem_provider {
public:
    virtual std::string name() const = 0;
    virtual std::shared_ptr<blockdev_filesystem> open(std::shared_ptr<blockdev> bdev) const = 0;
};

void init_filesystem_providers();
void add_filesystem_provider(std::shared_ptr<filesystem_provider> provider);
std::vector<std::string> get_filesystem_providers();
std::shared_ptr<blockdev_filesystem> open_filesystem(std::string provider, std::shared_ptr<blockdev> bdev);

void register_filesystem_providers();

#endif //FSBITS_FILESYSTEM_H
