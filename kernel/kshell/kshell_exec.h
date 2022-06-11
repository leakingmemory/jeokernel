//
// Created by sigsegv on 6/8/22.
//

#ifndef JEOKERNEL_KSHELL_EXEC_H
#define JEOKERNEL_KSHELL_EXEC_H

#include "kshell.h"

class kshell_exec : public kshell_command {
private:
    std::string command;
public:
    kshell_exec() : command("exec") {}
    const std::string &Command() const override {
        return command;
    }
    void Exec(kshell &, const std::vector<std::string> &cmd) override;
};


#endif //JEOKERNEL_KSHELL_EXEC_H
