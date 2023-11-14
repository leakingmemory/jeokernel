//
// Created by sigsegv on 10/18/23.
//

#include "mkdir.h"
#include "mockreferrer.h"
#include <iostream>

fsreference<directory> mkdir(const fsreference<directory> &rootdir, const std::string &ipath, bool pathCreate) {
    fsreference<fileitem> file{};
    std::string createFileName{};
    auto referrer = std::make_shared<mockreferrer>();

    {
        std::string path{ipath};
        while (path.ends_with('/')) {
            path.resize(path.size() - 1);
        }
        typeof(path.find_last_of('/')) slashpos;
        if ((slashpos = path.find_last_of('/')) < path.size()) {
            createFileName = path.substr(slashpos + 1);
            path.resize(slashpos);
            auto result = rootdir->Resolve(referrer, path);
            file = std::move(result.file);
            if (!file) {
                if (result.status == fileitem_status::SUCCESS) {
                    if (pathCreate) {
                        file = mkdir(rootdir, path, true);
                        if (!file) {
                            return {};
                        }
                    } else {
                        std::cerr << "Path not found: " << path << "\n";
                        return {};
                    }
                } else {
                    std::cerr << "Path error: " << path << ": " << text(result.status) << "\n";
                    return {};
                }
            }
        } else {
            createFileName = path;
            file = rootdir.CreateReference(referrer);
        }
    }
    if (createFileName.empty()) {
        std::cerr << "Trying to create empty filename\n";
        return {};
    }

    fsreference<directory> dir = fsreference_dynamic_cast<directory>(std::move(file));
    if (!dir) {
        std::cerr << "Not a directory\n";
        return {};
    }

    std::cout << "Will create " << createFileName << "\n";

    auto createResult = dir->CreateDirectory(referrer, createFileName, 00755);
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
        return {};
    }
    file = std::move(createResult.file);
    dir = fsreference_dynamic_cast<directory>(std::move(file));
    if (!dir) {
        std::cerr << "Not a directory\n";
        return {};
    }
    return dir;
}

int mkdir(const fsreference<directory> &rootdir, std::vector<std::string>::iterator &args, const std::vector<std::string>::iterator &args_end) {
    std::shared_ptr<fileitem> file{};
    std::string createFileName{};
    bool pathCreate{false};
    bool nextDir{false};

    while (args != args_end) {
        auto arg = *args;
        if (nextDir) {
            nextDir = false;
        } else if (arg == "-") {
            nextDir = true;
            ++args;
            continue;
        } else if (arg.starts_with("-")) {
            arg.erase(0, 1);
            for (auto opt : arg) {
                switch (opt) {
                    case 'p':
                        pathCreate = true;
                        break;
                    default:
                        std::cerr << "Invalid option: " << opt << "\n";
                        return 1;
                }
            }
            ++args;
            continue;
        }
        {
            std::string path = *args;
            if (!mkdir(rootdir, path, pathCreate)) {
                return 2;
            }
            ++args;
        }
    }
    return 0;
}
