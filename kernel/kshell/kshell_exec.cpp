//
// Created by sigsegv on 6/8/22.
//

#include <iostream>
#include <exec/exec.h>
#include "kshell_exec.h"

void kshell_exec::Exec(kshell &shell, const std::vector<std::string> &cmd) {
    std::shared_ptr<kfile> dir_ref{shell.CwdRef()};
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
            auto resolveResult = get_kernel_rootdir()->Resolve(resname);
            if (resolveResult.status != kfile_status::SUCCESS) {
                std::cerr << "Error: " << text(resolveResult.status) << "\n";
                return;
            }
            litem = resolveResult.result;
        } else {
            litem = get_kernel_rootdir();
        }
        if (litem) {
            kdirectory *ldir = dynamic_cast<kdirectory *> (&(*litem));
            if (ldir != nullptr) {
                std::cerr << "exec: is a directory: " << filename << "\n";
            } else {
                class Exec exec{shell.Tty(), dir_ref, *dir, litem, filename};
                exec.Run();
            }
        } else {
            std::cerr << "exec: not found: " << filename << "\n";
        }
    } else {
        auto resolveResult = dir->Resolve(filename);
        if (resolveResult.status != kfile_status::SUCCESS) {
            std::cerr << "Error: " << text(resolveResult.status) << "\n";
            return;
        }
        auto litem = resolveResult.result;
        if (litem) {
            kdirectory *ldir = dynamic_cast<kdirectory *> (&(*litem));
            if (ldir != nullptr) {
                std::cerr << "exec: is a directory: " << filename << "\n";
            } else {
                class Exec exec{shell.Tty(), dir_ref, *dir, litem, filename};
                exec.Run();
            }
        } else {
            std::cerr << "exec: not found: " << filename << "\n";
        }
    }
}