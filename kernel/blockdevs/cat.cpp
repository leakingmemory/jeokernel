//
// Created by sigsegv on 6/7/22.
//

#include <iostream>
#include "cat.h"
#include "mockreferrer.h"

int cat(const fsreference<directory> &rootdir, std::vector<std::string>::iterator &args, const std::vector<std::string>::iterator &args_end) {
    auto referrer = std::make_shared<mockreferrer>();
    fsreference<fileitem> file = rootdir.CreateReference(referrer);
    std::string final_filename{};
    while (args != args_end) {
        {
            std::string path = *args;
            ++args;
            auto result = rootdir->Resolve(referrer, path);
            file = std::move(result.file);
            if (!file) {
                if (result.status == fileitem_status::SUCCESS) {
                    std::cerr << "Path not found: " << path << "\n";
                } else {
                    std::cerr << "Path error: " << path << ": " << text(result.status) << "\n";
                }
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
                if (rd.size == 0) {
                    if (rd.status != fileitem_status::SUCCESS) {
                        std::cerr << "Read error: " << text(rd.status) << "\n";
                    }
                    break;
                }
                offset += rd.size;
                output.clear();
                output.append((const char *) ptr, rd.size);
                std::cout << output;
            }
        }
    }
    return 0;
}