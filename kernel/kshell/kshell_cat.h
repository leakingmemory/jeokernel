//
// Created by sigsegv on 6/7/22.
//

#ifndef JEOKERNEL_KSHELL_CAT_H
#define JEOKERNEL_KSHELL_CAT_H

#include "kshell.h"

class kshell_cat : public kshell_command {
private:
    std::string command;
    uint8_t buf[256];
public:
    kshell_cat() : command("cat") {}
    const std::string &Command() const override {
        return command;
    }
    void Exec(kshell &, const std::vector<std::string> &cmd) override;
};


#endif //JEOKERNEL_KSHELL_CAT_H
