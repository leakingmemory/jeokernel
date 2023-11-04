//
// Created by sigsegv on 6/4/22.
//

#include <iostream>
#include <files/symlink.h>
#include "ls.h"

int ls(std::shared_ptr<directory> rootdir, std::vector<std::string>::iterator &args, const std::vector<std::string>::iterator &args_end) {
    std::shared_ptr<fileitem> file = rootdir;
    std::string final_filename{};
    if (args != args_end) {
        std::string path = *args;
        ++args;
        std::cout << "Looking for path: " << path << "\n";
        auto fileResolve = rootdir->Resolve(path);
        file = fileResolve.file;
        if (!file) {
            if (fileResolve.status == fileitem_status::SUCCESS) {
                std::cerr << "Path not found: " << path << "\n";
            } else {
                std::cerr << "Path error: " << path << ": " << text(fileResolve.status) << "\n";
            }
            return 2;
        }
    }
    {
        directory *dir = dynamic_cast<directory*>(&(*file));
        if (dir != nullptr) {
            auto entriesResult = dir->Entries();
            if (entriesResult.status == fileitem_status::SUCCESS) {
                for (auto entry : entriesResult.entries) {
                    auto itemResult = entry->LoadItem();
                    if (itemResult.status == fileitem_status::SUCCESS) {
                        auto item = itemResult.file;
                        std::cout << std::oct << item->Mode() << std::dec << " " << item->Size() << " "
                                  << item->SysDevId()
                                  << ":" << item->InodeNum() << " " << entry->Name();
                        class symlink* syml = dynamic_cast<class symlink *>(&(*item));
                        if (syml == nullptr) {
                            std::cout << "\n";
                        } else {
                            std::cout << " -> " << syml->GetLink() << "\n";
                        }
                    } else {
                        std::cerr << "Directory entry: " << entry->Name() << ": " << text(itemResult.status) << "\n";
                    }
                }
            } else {
                std::cerr << "Directory error: " << text(entriesResult.status) << "\n";
            }
        } else {
            auto item = file;
            std::cout << std::oct << item->Mode() << std::dec << " " << item->Size() << " " << item->SysDevId() << ":" << item->InodeNum() << " " << final_filename << "\n";
        }
    }
    return 0;
}