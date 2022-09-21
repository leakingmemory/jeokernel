//
// Created by sigsegv on 4/25/22.
//

#ifndef FSBITS_FILESYSTEM_H
#define FSBITS_FILESYSTEM_H

#include <memory>
#include <vector>
#include <files/directory.h>

class blockdev;

enum class filesystem_status {
    SUCCESS,
    IO_ERROR,
    INTEGRITY_ERROR,
    NOT_SUPPORTED_FS_FEATURE,
    INVALID_REQUEST
};

std::string text(filesystem_status status);

template <typename T> struct filesystem_get_node_result {
    std::shared_ptr<T> node;
    filesystem_status status;
};

class filesystem {
public:
    virtual filesystem_get_node_result<directory> GetRootDirectory(std::shared_ptr<filesystem> shared_this) = 0;
};

class blockdev_filesystem : public filesystem {
protected:
    std::shared_ptr<blockdev> bdev;
public:
    blockdev_filesystem(std::shared_ptr<blockdev> bdev) : bdev(bdev) {
    }
    filesystem_get_node_result<directory> GetRootDirectory(std::shared_ptr<filesystem> shared_this) override = 0;
};

class filesystem_provider {
public:
    virtual std::string name() const = 0;
};

class special_filesystem_provider : public filesystem_provider {
public:
    virtual std::shared_ptr<filesystem> open() const = 0;
};

class blockdev_filesystem_provider : public filesystem_provider {
public:
    virtual std::shared_ptr<blockdev_filesystem> open(std::shared_ptr<blockdev> bdev) const = 0;
};

void init_filesystem_providers();
void add_filesystem_provider(std::shared_ptr<filesystem_provider> provider);
std::vector<std::string> get_filesystem_providers();
std::shared_ptr<filesystem> open_filesystem(std::string provider);
std::shared_ptr<blockdev_filesystem> open_filesystem(std::string provider, std::shared_ptr<blockdev> bdev);

void register_filesystem_providers();

#endif //FSBITS_FILESYSTEM_H
