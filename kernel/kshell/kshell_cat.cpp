//
// Created by sigsegv on 6/7/22.
//

#include <iostream>
#include "kshell_cat.h"

void kshell_cat::Exec(kshell &shell, const std::vector<std::string> &cmd) {
    std::shared_ptr<kdirectory> dir_ref{};
    kdirectory *dir = &(shell.Cwd());
    auto iterator = cmd.begin();
    if (iterator != cmd.end()) {
        ++iterator;
    }
    std::string output{};
    while (iterator != cmd.end()) {
        std::string filename = *iterator;
        if (filename.starts_with("/")) {
            std::string resname{};
            resname.append(filename.c_str()+1);
            while (resname.starts_with("/")) {
                std::string trim{};
                trim.append(resname.c_str()+1);
                resname = trim;
            }
            std::shared_ptr<kfile> litem;
            if (!resname.empty()) {
                litem = get_kernel_rootdir()->Resolve(resname);
            } else {
                litem = get_kernel_rootdir();
            }
            kdirectory *ldir = dynamic_cast<kdirectory *> (&(*litem));
            if (ldir != nullptr) {
                std::cerr << "cat: is a directory: " << filename << "\n";
            } else {
                uint64_t offset{0};
                while (true) {
                    auto rd = litem->Read(offset, &(buf[0]), sizeof(buf));
                    if (rd == 0) {
                        break;
                    }
                    offset += rd;
                    output.clear();
                    output.append((char *) &(buf[0]), rd);
                    std::cout << output;
                }
            }
        } else {
            auto litem = dir->Resolve(filename);
            kdirectory *ldir = dynamic_cast<kdirectory *> (&(*litem));
            if (ldir != nullptr) {
                std::cerr << "cat: is a directory: " << filename << "\n";
            } else {
                uint64_t offset{0};
                while (true) {
                    auto rd = litem->Read(offset, &(buf[0]), sizeof(buf));
                    if (rd == 0) {
                        break;
                    }
                    offset += rd;
                    output.clear();
                    output.append((char *) &(buf[0]), rd);
                    std::cout << output;
                }
            }
        }
        ++iterator;
    }
}