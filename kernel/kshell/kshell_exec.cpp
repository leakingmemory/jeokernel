//
// Created by sigsegv on 6/8/22.
//

#include <iostream>
#include <exec/exec.h>
#include "kshell_exec.h"

void kshell_exec::Exec(kshell &shell, const std::vector<std::string> &cmd) {
    std::shared_ptr<kdirectory> dir_ref{};
    kdirectory *dir = &(shell.Cwd());
    auto iterator = cmd.begin();
    if (iterator != cmd.end()) {
        ++iterator;
    }
    if (iterator == cmd.end()) {
        std::cerr << "No filename for exec\n";
    }
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
            std::cerr << "exec: is a directory: " << filename << "\n";
        } else {
            class Exec exec{litem};
            exec.Run();
        }
    } else {
        auto litem = dir->Resolve(filename);
        kdirectory *ldir = dynamic_cast<kdirectory *> (&(*litem));
        if (ldir != nullptr) {
            std::cerr << "exec: is a directory: " << filename << "\n";
        } else {
            class Exec exec{litem};
            exec.Run();
        }
    }
}