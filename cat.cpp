//
// Created by sigsegv on 6/7/22.
//

#include <iostream>
#include "cat.h"

int cat(std::shared_ptr<directory> rootdir, std::vector<std::string>::iterator &args, const std::vector<std::string>::iterator &args_end) {
    std::shared_ptr<fileitem> file = rootdir;
    std::string final_filename{};
    while (args != args_end) {
        {
            std::string path = *args;
            ++args;
            file = rootdir->Resolve(path);
            if (!file) {
                std::cerr << "Path not found: " << path << "\n";
                return 2;
            }
        }

        directory *dir = dynamic_cast<directory*>(&(*file));
        if (dir != nullptr) {
            std::cerr << "cat: Is a directory\n";
        } else {
            std::string output{};
            uint64_t offset{0};
            void *ptr = malloc(256);
            while (true) {
                auto rd = file->Read(offset, ptr, 256);
                if (rd == 0) {
                    break;
                }
                offset += rd;
                output.clear();
                output.append((const char *) ptr, rd);
                std::cout << output;
            }
        }
    }
    return 0;
}