//
// Created by sigsegv on 9/13/23.
//

#include "create.h"
#include <iostream>

int create(std::shared_ptr<directory> rootdir, std::vector<std::string>::iterator &args, const std::vector<std::string>::iterator &args_end) {
    std::shared_ptr<fileitem> file{};
    std::string createFileName{};
    while (args != args_end) {
        {
            std::string path = *args;
            createFileName = path;
            ++args;
            while (path.ends_with('/')) {
                path.resize(path.size() - 1);
            }
            typeof(path.find_last_of('/')) slashpos;
            if ((slashpos = path.find_last_of('/')) < path.size()) {
                createFileName = path.substr(slashpos + 1);
                path.resize(slashpos);
                auto result = rootdir->Resolve(path);
                file = result.file;
                if (!file) {
                    if (result.status == fileitem_status::SUCCESS) {
                        std::cerr << "Path not found: " << path << "\n";
                    } else {
                        std::cerr << "Path error: " << path << ": " << text(result.status) << "\n";
                    }
                    return 2;
                }
            } else {
                file = rootdir;
            }
        }
        if (createFileName.empty()) {
            std::cerr << "Trying to create empty filename\n";
            return 3;
        }

        std::shared_ptr<directory> dir = std::dynamic_pointer_cast<directory>(file);
        if (!dir) {
            std::cerr << "Not a directory\n";
            continue;
        }

        std::cout << "Will create " << createFileName << "\n";

        auto createResult = dir->Create(createFileName, 00644);
        if (!createResult.file || createResult.status != fileitem_status::SUCCESS) {
            switch (createResult.status) {
                case fileitem_status::IO_ERROR:
                    std::cerr << "create: I/O error\n";
                    break;
                case fileitem_status::INTEGRITY_ERROR:
                    std::cerr << "create: Integrity error\n";
                    break;
                case fileitem_status::NOT_SUPPORTED_FS_FEATURE:
                    std::cerr << "create: Not supported feature\n";
                    break;
                case fileitem_status::INVALID_REQUEST:
                    std::cerr << "create: Invalid request\n";
                    break;
                case fileitem_status::TOO_MANY_LINKS:
                    std::cerr << "create: Too many links\n";
                    break;
                case fileitem_status::NO_AVAIL_INODES:
                    std::cerr << "create: No available inodes\n";
                    break;
                default:
                    std::cerr << "create: Unknown error\n";
            }
            continue;
        }
    }
    return 0;
}
