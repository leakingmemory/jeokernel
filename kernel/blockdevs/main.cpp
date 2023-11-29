#include <fileaccess/file_blockdev.h>
#include <blockdevs/parttable_readers.h>
#include <blockdevs/offset_blockdev.h>
#include <filesystems/filesystem.h>
#include <files/fsresource.h>
#include <iostream>
#include <cstring>
#include <vector>
#include <string>
#include "ls.h"
#include "cat.h"
#include "create.h"
#include "mkdir.h"
#include "mockreferrer.h"

bool is_fswrite(std::vector<std::string>::iterator args, const std::vector<std::string>::iterator &args_end) {
    if (args != args_end) {
        auto cmd = *args;
        ++args;
        if (cmd == "create") {
            return true;
        }
        if (cmd == "mkdir") {
            return true;
        }
    }
    return false;
}

int fsmain(const fsreference<directory> &rootdir, std::vector<std::string>::iterator &args, const std::vector<std::string>::iterator &args_end) {
    if (args != args_end) {
        auto cmd = *args;
        ++args;
        if (cmd == "ls") {
            return ls(rootdir, args, args_end);
        } else if (cmd == "cat") {
            return cat(rootdir, args, args_end);
        } else if (cmd == "create") {
            return create(rootdir, args, args_end);
        } else if (cmd == "mkdir") {
            return mkdir(rootdir, args, args_end);
        } else {
            std::cerr << "Invalid command: " << cmd << "\n";
            return 1;
        }
    }
    auto referrer = std::make_shared<mockreferrer>();
    auto entriesResult = rootdir->Entries();
    if (entriesResult.status == fileitem_status::SUCCESS) {
        for (auto entry: entriesResult.entries) {
            auto itemResult = entry->LoadItem(referrer);
            if (itemResult.status == fileitem_status::SUCCESS) {
                auto item = std::move(itemResult.file);
                std::cout << std::oct << item->Mode() << std::dec << " " << item->Size() << " " << entry->Name()
                          << "\n";
            } else {
                std::cerr << "Directory entry: " << entry->Name() << ": " << text(itemResult.status) << "\n";
            }
        }
    } else {
        std::cerr << "Root dir error: " << text(entriesResult.status) << "\n";
    }
    return 0;
}

void writetodev(std::shared_ptr<blockdev> bdev, const std::vector<dirty_block> &grp) {
    auto blocksize = bdev->GetBlocksize();
    for (const auto &wr : grp) {
        std::cout << "  Write block " << wr.blockaddr << " offset=" << wr.offset << " length=" << wr.length
                  << std::hex << " ptr1=" << ((uintptr_t) wr.page1->Pointer()->Pointer()) << " ptr2="
                  << (wr.page2 ? ((uintptr_t) wr.page2->Pointer()->Pointer()) : 0) << std::dec << "\n";
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
                    std::cerr << "Buffer overrun\n";
                    throw std::exception();
                }
                wrBlocks = bdev->WriteBlock(
                        ((uint8_t *) (wr.page2->Pointer()->Pointer())) + offset - FILEPAGE_PAGE_SIZE,
                        blockaddr, blocks);
            } else {
                chunkLength = blocksize;
                void *buf = malloc(blocksize);
                if (buf == nullptr) {
                    std::cerr << "Memory alloc\n";
                    throw std::exception();
                }
                auto endLength = FILEPAGE_PAGE_SIZE - offset;
                memcpy(buf, ((uint8_t *) (wr.page1->Pointer()->Pointer())) + offset, endLength);
                memcpy(((uint8_t *) buf) + endLength, wr.page2->Pointer()->Pointer(), chunkLength - endLength);
                wrBlocks = bdev->WriteBlock(buf, blockaddr, 1);
                free(buf);
            }
            if (wrBlocks <= 0) {
                std::cerr << "Write error\n";
                throw std::exception();
            }
            blockaddr += wrBlocks;
            i += wrBlocks * blocksize;
        }
    }
}
void writetodev(std::shared_ptr<blockdev> bdev, std::vector<std::vector<dirty_block>> &writes) {
    for (const auto &grp : writes) {
        std::cout << "Write blocks grouping:\n";
        writetodev(bdev, grp);
    }
}

class fsbits_fsresourcelock : public fsresourcelock {
private:
    std::mutex mtx{};
public:
    void lock() override;
    void unlock() override;
};

void fsbits_fsresourcelock::lock() {
    mtx.lock();
}

void fsbits_fsresourcelock::unlock() {
    mtx.unlock();
}

class fsbits_fsresourcelockfactory : public fsresourcelockfactory {
public:
    std::shared_ptr<fsresourcelock> Create() override;
};

std::shared_ptr<fsresourcelock> fsbits_fsresourcelockfactory::Create() {
    return std::make_shared<fsbits_fsresourcelock>();
}

int blockdevmain(std::shared_ptr<blockdev> bdev, std::string fsname, std::vector<std::string>::iterator &args, const std::vector<std::string>::iterator &args_end) {
    std::shared_ptr<blockdev_filesystem> bdev_fs = open_filesystem(std::make_shared<fsbits_fsresourcelockfactory>(), fsname, bdev);
    std::shared_ptr<filesystem> fs = bdev_fs;
    if (!fs) {
        std::cerr << "Failed to open filesystem " << fsname << "\n";
        return 1;
    }
    auto referrer = std::make_shared<mockreferrer>();
    fsreference<directory> rootdir{};
    {
        auto rootdirResult = fs->GetRootDirectory(fs, referrer);
        rootdir = std::move(rootdirResult.node);
        if (!rootdir) {
            if (rootdirResult.status == filesystem_status::SUCCESS) {
                std::cerr << "Failed to open filesystem root directory " << fsname << "\n";
            } else {
                std::cerr << "Rootdir error on " << fsname << ": " << text(rootdirResult.status) << "\n";
            }
            return 1;
        }
    }
    auto isWrite = is_fswrite(args, args_end);
    if (isWrite) {
        auto writes = bdev_fs->OpenForWrite();
        writetodev(bdev, writes);
    }
    auto result = fsmain(rootdir, args, args_end);
    rootdir = {};
    {
        auto writes = bdev_fs->GetWrites();
        writetodev(bdev, writes);
    }
    while (true) {
        auto writes = bdev_fs->FlushOrClose();
        if (!writes.blocks.empty()) {
            writetodev(bdev, writes.blocks);
        }
        if (writes.completed) {
            break;
        }
    }
    return result;
}

static int sysDevIds = 1;

int cppmain(std::vector<std::string> args) {
    auto iterator = args.begin();
    if (iterator == args.end()) {
        return 0;
    }
    std::string cmd = *iterator;
    if (++iterator == args.end()) {
        return 0;
    }
    std::string blockdev_name = *iterator;
    if (++iterator == args.end()) {
        return 0;
    }
    std::size_t blocksize{0};
    {
        std::string blksz = *iterator;
        char *endp;
        blocksize = strtol(blksz.c_str(), &endp, 10);
        if (*endp != '\0') {
            std::cerr << "Invalid blocksize: " << blksz << "\n";
            return 1;
        }
    }
    std::shared_ptr<file_blockdev> fblockdev{new file_blockdev(blockdev_name, blocksize, 1)};
    std::cout << blockdev_name << " blocksize " << blocksize << " - partitions:\n";
    parttable_readers parttableReaders{};
    auto parttable = parttableReaders.ReadParttable(fblockdev);
    if (parttable) {
        std::cout << "Partition table of type " << parttable->GetTableType() << ":\n";
        for (auto entry: parttable->GetEntries()) {
            std::cout << " " << entry->GetOffset() << ", " << entry->GetSize() << ", " << entry->GetType() << "\n";
        }
    }
    if (++iterator == args.end()) {
        return 0;
    }
    std::shared_ptr<blockdev> fs_blockdev = fblockdev;
    if (parttable) {
        std::string i_part = *iterator;
        char *endp;
        int part_idx = strtol(i_part.c_str(), &endp, 10);
        if (*endp != '\0' || part_idx < 0 || part_idx >= parttable->GetEntries().size()) {
            std::cerr << "Invalud partition index: " << i_part << "\n";
            return 1;
        }
        if (++iterator == args.end()) {
            return 0;
        }
        auto part_entry = parttable->GetEntries()[part_idx];
        std::shared_ptr<blockdev> partdev{new offset_blockdev(fblockdev, part_entry->GetOffset(), part_entry->GetSize(), ++sysDevIds)};
        fs_blockdev = partdev;
    }
    std::string fs_name = *iterator;
    {
        bool found = false;
        auto filesystems = get_filesystem_providers();
        for (auto provider: filesystems) {
            if (fs_name == provider) {
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << fs_name << " is not a valid filesystem name. Available filesystems:\n";
            for (auto provider: filesystems) {
                std::cerr << " - " << provider << "\n";
            }
            return 1;
        }
    }
    ++iterator;
    return blockdevmain(fs_blockdev, fs_name, iterator, args.end());
}

int main(int argc, char **argv) {
    std::vector<std::string> args{};
    for (int i = 0; i < argc; i++) {
        args.push_back(std::string(argv[i]));
    }

    init_filesystem_providers();
    register_filesystem_providers();

    return cppmain(args);
}
