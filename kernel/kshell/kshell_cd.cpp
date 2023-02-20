//
// Created by sigsegv on 6/7/22.
//

#include <iostream>
#include "kshell_cd.h"

void kshell_cd::Exec(kshell &shell, const std::vector<std::string> &cmd) {
    auto cwd = shell.Cwd();
    auto iterator = cmd.begin();
    if (iterator != cmd.end()) {
        ++iterator;
    }
    auto rootdir = get_kernel_rootdir();
    if (iterator == cmd.end()) {
        shell.Cwd(rootdir);
        return;
    }
    std::string dir = *iterator;
    ++iterator;
    if (iterator != cmd.end()) {
        std::cerr << "cd: Too many arguments\n";
        return;
    }
    kfile_result<std::shared_ptr<kfile>> newDirRes{};
    if (dir.starts_with("/")) {
        while (dir.starts_with("/")) {
            std::string trim{};
            trim.append(dir.c_str() + 1);
            dir = trim;
        }
        newDirRes = rootdir->Resolve(&(*rootdir), dir);
    } else {
        newDirRes = cwd.Resolve(&(*rootdir), dir);
    }
    if (newDirRes.status != kfile_status::SUCCESS) {
        std::cerr << "Error: " << text(newDirRes.status) << "\n";
        return;
    }
    auto newDir = newDirRes.result;
    if (!newDir) {
        std::cerr << "cd: Not found\n";
        return;
    }
    if (dynamic_cast<kdirectory *>(&(*newDir)) == nullptr) {
        std::cerr << "cd: Not a directory\n";
        return;
    }
    shell.Cwd(newDir);
}