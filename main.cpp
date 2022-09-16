#include <fileaccess/file_blockdev.h>
#include <blockdevs/parttable_readers.h>
#include <blockdevs/offset_blockdev.h>
#include <filesystems/filesystem.h>
#include <iostream>
#include <vector>
#include <string>
#include "ls.h"
#include "cat.h"

int blockdevmain(std::shared_ptr<blockdev> bdev, std::string fsname, std::vector<std::string>::iterator &args, const std::vector<std::string>::iterator &args_end) {
    std::shared_ptr<filesystem> fs = open_filesystem(fsname, bdev);
    if (!fs) {
        std::cerr << "Failed to open filesystem " << fsname << "\n";
        return 1;
    }
    std::shared_ptr<directory> rootdir = fs->GetRootDirectory(fs);
    if (!rootdir) {
        std::cerr << "Failed to open filesystem root directory " << fsname << "\n";
        return 1;
    }
    if (args != args_end) {
        auto cmd = *args;
        ++args;
        if (cmd == "ls") {
            return ls(rootdir, args, args_end);
        } else if (cmd == "cat") {
            return cat(rootdir, args, args_end);
        } else {
            std::cerr << "Invalid command: " << cmd << "\n";
            return 1;
        }
    }
    for (auto entry : rootdir->Entries()) {
        auto item = entry->Item();
        std::cout << std::oct << item->Mode() << std::dec << " " << item->Size() << " " << entry->Name() << "\n";
    }
    return 0;
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
