#include <fileaccess/file_blockdev.h>
#include <blockdevs/parttable_readers.h>
#include <iostream>
#include <vector>
#include <string>

int cppmain(std::vector<std::string> args) {
    auto iterator = args.begin();
    if (iterator == args.end()) {
        return 0;
    }
    std::string cmd = *iterator;
    if (++iterator == args.end()) {
        return 0;
    }
    std::string blockdev = *iterator;
    if (++iterator == args.end()) {
        return 0;
    }
    std::size_t blocksize{0};
    {
        std::string blksz = *iterator;
        char *endp;
        blocksize = strtol(blksz.c_str(), &endp, 10);
        if (*endp != '\0') {
            std::cerr << "Invalud blocksize: " << blksz << "\n";
            return 1;
        }
    }
    std::shared_ptr<file_blockdev> fblockdev{new file_blockdev(blockdev, blocksize)};
    std::cout << blockdev << " blocksize " << blocksize << " - partitions:\n";
    parttable_readers parttableReaders{};
    auto parttable = parttableReaders.ReadParttable(fblockdev);
    std::cout << "Partition table of type " << parttable->GetTableType() << ":\n";
    for (auto entry : parttable->GetEntries()) {
        std::cout << " " << entry->GetOffset() << ", " << entry->GetSize() << ", " << entry->GetType() << "\n";
    }
    return 0;
}

int main(int argc, char **argv) {
    std::vector<std::string> args{};
    for (int i = 0; i < argc; i++) {
        args.push_back(std::string(argv[i]));
    }
    return cppmain(args);
}
