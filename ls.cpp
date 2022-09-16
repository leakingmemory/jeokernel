//
// Created by sigsegv on 6/4/22.
//

#include <iostream>
#include "ls.h"

int ls(std::shared_ptr<directory> rootdir, std::vector<std::string>::iterator &args, const std::vector<std::string>::iterator &args_end) {
    std::shared_ptr<fileitem> file = rootdir;
    std::string final_filename{};
    if (args != args_end) {
        std::string path = *args;
        ++args;
        std::cout << "Looking for path: " << path << "\n";
        file = rootdir->Resolve(path);
        if (!file) {
            std::cerr << "Path not found: " << path << "\n";
            return 2;
        }
    }
    {
        directory *dir = dynamic_cast<directory*>(&(*file));
        if (dir != nullptr) {
            for (auto entry : dir->Entries()) {
                auto item = entry->Item();
                std::cout << std::oct << item->Mode() << std::dec << " " << item->Size() << " " << item->SysDevId() << ":" << item->InodeNum() << " " << entry->Name() << "\n";
            }
        } else {
            auto item = file;
            std::cout << std::oct << item->Mode() << std::dec << " " << item->Size() << " " << item->SysDevId() << ":" << item->InodeNum() << " " << final_filename << "\n";
        }
    }
    return 0;
}