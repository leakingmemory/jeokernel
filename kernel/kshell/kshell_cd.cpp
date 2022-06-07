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
    if (iterator == cmd.end()) {
        shell.Cwd(get_kernel_rootdir());
        return;
    }
    std::string dir = *iterator;
    ++iterator;
    if (iterator != cmd.end()) {
        std::cerr << "cd: Too many arguments\n";
        return;
    }
    std::shared_ptr<kfile> newDir{};
    if (dir.starts_with("/")) {
        while (dir.starts_with("/")) {
            std::string trim{};
            trim.append(dir.c_str() + 1);
            dir = trim;
        }
        newDir = get_kernel_rootdir()->Resolve(dir);
    } else {
        newDir = cwd.Resolve(dir);
    }
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