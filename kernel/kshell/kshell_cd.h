//
// Created by sigsegv on 6/7/22.
//

#ifndef JEOKERNEL_KSHELL_CD_H
#define JEOKERNEL_KSHELL_CD_H

#include "kshell.h"

class kshell_cd : public kshell_command {
private:
    std::string command;
public:
    kshell_cd() : command("cd") {}
    const std::string &Command() const override {
        return command;
    }
    void Exec(kshell &, const std::vector<std::string> &cmd) override;
};


#endif //JEOKERNEL_KSHELL_CD_H
