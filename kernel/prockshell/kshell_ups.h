//
// Created by sigsegv on 12/2/23.
//

#ifndef JEOKERNEL_KSHELL_UPS_H
#define JEOKERNEL_KSHELL_UPS_H

#include <kshell/kshell.h>

class kshell_ups : public kshell_command {
private:
    std::string command;
public:
    kshell_ups() : command("ups") {}
    [[nodiscard]] const std::string &Command() const override {
        return command;
    }
    void Exec(kshell &, const std::vector<std::string> &cmd) override;
};


#endif //JEOKERNEL_KSHELL_UPS_H
