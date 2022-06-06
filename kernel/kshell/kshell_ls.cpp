//
// Created by sigsegv on 6/6/22.
//

#include <iostream>
#include "kshell_ls.h"

void kshell_ls::Exec(kshell &shell, const std::vector<std::string> &cmd) {
    std::shared_ptr<kdirectory> dir_ref{};
    kdirectory *dir = &(shell.Cwd());
    for (auto entry : dir->Entries()) {
        auto file = entry->File();
        std::cout << std::oct << file->Mode() << std::dec << " " << file->Size() << " " << entry->Name() << "\n";
    }
}
