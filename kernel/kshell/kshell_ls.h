//
// Created by sigsegv on 6/6/22.
//

#ifndef JEOKERNEL_KSHELL_LS_H
#define JEOKERNEL_KSHELL_LS_H

#include "kshell.h"

class kshell_ls : public kshell_command {
private:
    std::string command;
public:
    kshell_ls() : command("ls") {}
    const std::string &Command() const override {
        return command;
    }
    void Exec(kshell &, const std::vector<std::string> &cmd) override;
};

#endif //JEOKERNEL_KSHELL_LS_H
