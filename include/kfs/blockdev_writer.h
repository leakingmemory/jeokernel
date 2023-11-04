//
// Created by sigsegv on 10/30/23.
//

#ifndef JEOKERNEL_BLOCKDEV_WRITER_H
#define JEOKERNEL_BLOCKDEV_WRITER_H

#include <memory>
#include <vector>
#include <thread>

class filesystem;
class blockdev_filesystem;
struct blockdev_writeable_filesystem;

class blockdev_writer {
private:
    std::thread collectWritesThread;
    std::thread submitWritesThread;
    std::vector<std::shared_ptr<blockdev_writeable_filesystem>> filesystems{};
    std::vector<std::shared_ptr<blockdev_writeable_filesystem>> flush{};
    std::vector<std::shared_ptr<blockdev_writeable_filesystem>> finish{};
    bool stop;
public:
    static blockdev_writer &GetInstance();
    blockdev_writer();
    blockdev_writer(const blockdev_writer &) = delete;
    blockdev_writer(blockdev_writer &&) = delete;
    blockdev_writer &operator =(const blockdev_writer &) = delete;
    blockdev_writer &operator =(blockdev_writer &&) = delete;
    ~blockdev_writer();
    void Collect();
    void Submit();
    void OpenForWrite(const std::shared_ptr<blockdev_filesystem> &fs);
    void OpenForWrite(const std::shared_ptr<filesystem> &fs);
    void CloseForWrite(const std::shared_ptr<blockdev_filesystem> &fs);
    void CloseForWrite(const std::shared_ptr<filesystem> &fs);
};


#endif //JEOKERNEL_BLOCKDEV_WRITER_H
