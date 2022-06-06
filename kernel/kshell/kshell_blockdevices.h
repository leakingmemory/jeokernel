//
// Created by sigsegv on 4/3/22.
//

#ifndef JEOKERNEL_KSHELL_BLOCKDEVICES_H
#define JEOKERNEL_KSHELL_BLOCKDEVICES_H

#include "kshell.h"

class kshell_blockdevices : public kshell_command {
private:
    std::string command;
public:
    kshell_blockdevices() : command("blockdevices") {}
    const std::string &Command() const override {
        return command;
    }
    void Exec(kshell &, const std::vector<std::string> &cmd) override;
};


#endif //JEOKERNEL_KSHELL_BLOCKDEVICES_H
