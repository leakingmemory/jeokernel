//
// Created by sigsegv on 10/30/23.
//

#include <kfs/blockdev_writer.h>
#include <memory>
#include <concurrency/hw_spinlock.h>
#include <mutex>
#include <filesystems/filesystem.h>
#include <blockdevs/blockdev.h>
#include <iostream>

static hw_spinlock bdevwriterinstance{};
static std::unique_ptr<blockdev_writer> bdevwriter{};

struct blockdev_writeable_filesystem {
    std::shared_ptr<blockdev_filesystem> fs{};
    std::vector<std::vector<dirty_block>> dirtyBlocks{};
    std::vector<dirty_block> flushNow{};
};

blockdev_writer &blockdev_writer::GetInstance() {
    std::lock_guard lock{bdevwriterinstance};
    if (bdevwriter) {
        return *bdevwriter;
    }
    bdevwriter = std::make_unique<blockdev_writer>();
    return *bdevwriter;
}

blockdev_writer::blockdev_writer() :
    collectWritesThread([this] () { Collect(); }),
    submitWritesThread([this] () { Submit(); })
{
}

blockdev_writer::~blockdev_writer() {
    {
        std::lock_guard lock{bdevwriterinstance};
        stop = true;
    }
    collectWritesThread.join();
    submitWritesThread.join();
}

void blockdev_writer::Collect() {
    while (true) {
        {
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(10s);
        }
        std::vector<std::shared_ptr<blockdev_writeable_filesystem>> filesystems{};
        std::vector<std::shared_ptr<blockdev_writeable_filesystem>> flushFilesystems{};
        {
            std::lock_guard lock{bdevwriterinstance};
            filesystems = this->filesystems;
            flushFilesystems = this->flush;
        }
        for (auto &fsw: filesystems) {
            auto blocks = fsw->fs->GetWrites();
            auto inIterator = blocks.begin();
            std::lock_guard lock{bdevwriterinstance};
            auto outIterator = fsw->dirtyBlocks.begin();
            while (inIterator != blocks.end() && outIterator != fsw->dirtyBlocks.end()) {
                auto &inList = *inIterator;
                auto &outList = *outIterator;
                for (auto &inItem: inList) {
                    outList.push_back(inItem);
                }
                ++inIterator;
                ++outIterator;
            }
            while (inIterator != blocks.end()) {
                auto &inList = *inIterator;
                fsw->dirtyBlocks.push_back(inList);
                ++inIterator;
            }
        }
        std::vector<std::shared_ptr<blockdev_writeable_filesystem>> flushedFilesystems{};
        for (auto &fsw : flushFilesystems) {
            auto result = fsw->fs->FlushOrClose();
            auto outIterator = fsw->dirtyBlocks.begin();
            if (outIterator != fsw->dirtyBlocks.end()) {
                auto &list = *outIterator;
                for (auto &db : result.blocks) {
                    list.push_back(db);
                }
            } else {
                auto &list = fsw->dirtyBlocks.emplace_back();
                for (auto &db : result.blocks) {
                    list.push_back(db);
                }
            }
            if (result.completed) {
                flushedFilesystems.push_back(fsw);
            }
        }
        if (!flushedFilesystems.empty()) {
            std::lock_guard lock{bdevwriterinstance};
            for (auto &fsw : flushedFilesystems) {
                auto iterator = flush.begin();
                while (iterator != flush.end()) {
                    auto &ff = *iterator;
                    if (fsw == ff) {
                        std::cout << "Filesystem writes on queue\n";
                        flush.erase(iterator);
                        finish.push_back(fsw);
                    } else {
                        ++iterator;
                    }
                }
            }
        }
    }
}

struct blockdev_dirty_block {
    std::shared_ptr<blockdev> bdev;
    dirty_block dirty;
};

void blockdev_writer::Submit() {
    bool idle{true};
    while (true) {
        if (idle) {
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(10s);
        }
        std::vector<std::shared_ptr<blockdev_writeable_filesystem>> filesystems{};
        {
            std::lock_guard lock{bdevwriterinstance};
            filesystems = this->filesystems;
            for (auto &fsw : this->flush) {
                filesystems.push_back(fsw);
            }
            auto iterator = this->finish.begin();
            while (iterator != this->finish.end()) {
                auto &fsw = *iterator;
                if (fsw->dirtyBlocks.empty() && fsw->flushNow.empty()) {
                    std::cout << "Filesystem flushed and closed\n";
                    this->finish.erase(iterator);
                } else {
                    filesystems.push_back(fsw);
                    ++iterator;
                }
            }
        }
        std::vector<blockdev_dirty_block> flushNow{};
        {
            std::lock_guard lock{bdevwriterinstance};
            for (auto &fsw: filesystems) {
                while (fsw->flushNow.empty() && !fsw->dirtyBlocks.empty()) {
                    auto iterator = fsw->dirtyBlocks.begin();
                    fsw->flushNow = *iterator;
                    fsw->dirtyBlocks.erase(iterator);
                }
                for (auto &flushItem : fsw->flushNow) {
                    blockdev_dirty_block bdirty{
                        .bdev = fsw->fs->GetBlockdev(),
                        .dirty = flushItem
                    };
                    flushNow.push_back(bdirty);
                }
                fsw->flushNow.clear();
            }
        }
        for (auto &flushItem : flushNow) {
            auto &bdev = flushItem.bdev;
            auto blocksize = bdev->GetBlocksize();
            auto &wr = flushItem.dirty;
            auto blockaddr = wr.blockaddr;
            auto offset = wr.offset;
            std::remove_const<typeof(wr.length)>::type i = 0;
            while (i < wr.length) {
                auto chunkLength = wr.length - i;
                if (chunkLength < blocksize) {
                    chunkLength = blocksize;
                }
                {
                    auto overshoot = chunkLength % blocksize;
                    if (overshoot != 0) {
                        chunkLength += blocksize - overshoot;
                    }
                }
                const auto blocks = chunkLength / blocksize;
                std::remove_const<typeof(blocks)>::type wrBlocks;
                if ((chunkLength + offset) <= FILEPAGE_PAGE_SIZE) {
                    wrBlocks = bdev->WriteBlock(
                            ((uint8_t *) (wr.page1->Pointer()->Pointer())) + offset,
                            blockaddr, blocks);
                } else if (offset >= FILEPAGE_PAGE_SIZE) {
                    if (!wr.page2) {
                        std::cerr << "Buffer overrun (fsync)\n";
                        break;
                    }
                    wrBlocks = bdev->WriteBlock(
                            ((uint8_t *) (wr.page2->Pointer()->Pointer())) + offset - FILEPAGE_PAGE_SIZE,
                            blockaddr, blocks);
                } else {
                    chunkLength = blocksize;
                    void *buf = malloc(blocksize);
                    if (buf == nullptr) {
                        std::cerr << "Memory alloc (fsync)\n";
                        break;
                    }
                    auto endLength = FILEPAGE_PAGE_SIZE - offset;
                    memcpy(buf, ((uint8_t *) (wr.page1->Pointer()->Pointer())) + offset, endLength);
                    memcpy(((uint8_t *) buf) + endLength, wr.page2->Pointer()->Pointer(), chunkLength - endLength);
                    wrBlocks = bdev->WriteBlock(buf, blockaddr, 1);
                    free(buf);
                }
                if (wrBlocks <= 0) {
                    std::cerr << "Write error (fsync)\n";
                    break;
                }
                blockaddr += wrBlocks;
                i += wrBlocks * blocksize;
            }
        }
        idle = flushNow.empty();
    }
}

void blockdev_writer::OpenForWrite(const std::shared_ptr<blockdev_filesystem> &fs) {
    auto blocks = fs->OpenForWrite();
    std::shared_ptr<blockdev_writeable_filesystem> fsw = std::make_shared<blockdev_writeable_filesystem>();
    fsw->fs = fs;
    fsw->flushNow = blocks;
    std::lock_guard lock{bdevwriterinstance};
    filesystems.push_back(fsw);
}

void blockdev_writer::OpenForWrite(const std::shared_ptr<filesystem> &fs) {
    auto bfs = std::dynamic_pointer_cast<blockdev_filesystem>(fs);
    if (bfs) {
        OpenForWrite(bfs);
    }
}

void blockdev_writer::CloseForWrite(const std::shared_ptr<blockdev_filesystem> &fs) {
    std::lock_guard lock{bdevwriterinstance};
    auto iterator = filesystems.begin();
    while (iterator != filesystems.end()) {
        std::shared_ptr<blockdev_writeable_filesystem> fsw = *iterator;
        if (fsw->fs == fs) {
            std::cout << "Queueing filesystem for flush\n";
            filesystems.erase(iterator);
            flush.push_back(fsw);
            return;
        }
        ++iterator;
    }
}

void blockdev_writer::CloseForWrite(const std::shared_ptr<filesystem> &fs) {
    auto bfs = std::dynamic_pointer_cast<blockdev_filesystem>(fs);
    if (bfs) {
        CloseForWrite(bfs);
    }
}
