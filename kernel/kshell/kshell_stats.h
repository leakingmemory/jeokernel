//
// Created by sigsegv on 3/9/22.
//

#ifndef JEOKERNEL_KSHELL_STATS_H
#define JEOKERNEL_KSHELL_STATS_H

#include "kshell.h"

class kshell_stats : public kshell_command {
private:
    std::string command;
public:
    kshell_stats() : command("stats") {}
    const std::string &Command() const override {
        return command;
    }
    void Exec(kshell &, const std::vector<std::string> &cmd) override;
};


#endif //JEOKERNEL_KSHELL_STATS_H
