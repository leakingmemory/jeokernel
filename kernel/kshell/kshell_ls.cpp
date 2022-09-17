//
// Created by sigsegv on 6/6/22.
//

#include <iostream>
#include "kshell_ls.h"

static void single(kfile *file, std::string name) {
    std::cout << std::oct << file->Mode() << std::dec << " " << file->Size() << " " << name << "\n";
}

static void list(kdirectory *dir) {
    auto result = dir->Entries();
    if (result.status != kfile_status::SUCCESS) {
        std::cerr << "Error: " << text(result.status) << "\n";
    }
    for (auto entry : result.result) {
        auto file = entry->File();
        single(&(*file), entry->Name());
    }
}

void kshell_ls::Exec(kshell &shell, const std::vector<std::string> &cmd) {
    std::shared_ptr<kdirectory> dir_ref{};
    kdirectory *dir = &(shell.Cwd());
    auto iterator = cmd.begin();
    if (iterator != cmd.end()) {
        ++iterator;
    }
    if (iterator != cmd.end()) {
        do {
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
                    auto resolveResult = get_kernel_rootdir()->Resolve(resname);
                    if (resolveResult.status != kfile_status::SUCCESS) {
                        std::cerr << "Error: " << text(resolveResult.status) << "\n";
                        return;
                    }
                    litem = resolveResult.result;
                } else {
                    litem = get_kernel_rootdir();
                }
                kdirectory *ldir = dynamic_cast<kdirectory *> (&(*litem));
                if (ldir != nullptr) {
                    list(ldir);
                } else {
                    single(&(*litem), filename);
                }
            } else {
                auto resolveResult = dir->Resolve(filename);
                if (resolveResult.status != kfile_status::SUCCESS) {
                    std::cerr << "Error: " << text(resolveResult.status) << "\n";
                    return;
                }
                auto litem = resolveResult.result;
                kdirectory *ldir = dynamic_cast<kdirectory *> (&(*litem));
                if (ldir != nullptr) {
                    list(ldir);
                } else {
                    single(&(*litem), filename);
                }
            }
            ++iterator;
        } while (iterator != cmd.end());
    } else {
        list(dir);
    }
}
