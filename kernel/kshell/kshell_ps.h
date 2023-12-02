//
// Created by sigsegv on 1/30/22.
//

#ifndef JEOKERNEL_KSHELL_PS_H
#define JEOKERNEL_KSHELL_PS_H

#include <kshell/kshell.h>

class kshell_ps : public kshell_command {
private:
    std::string command;
public:
    kshell_ps() : command("ps") {}
    const std::string &Command() const override {
        return command;
    }
    void Exec(kshell &, const std::vector<std::string> &cmd) override;
};


#endif //JEOKERNEL_KSHELL_PS_H
