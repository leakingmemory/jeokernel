//
// Created by sigsegv on 11/3/23.
//

#ifndef JEOKERNEL_KSHELL_UMOUNT_H
#define JEOKERNEL_KSHELL_UMOUNT_H

#include <kshell/kshell.h>

class kshell_umount : public kshell_command {
private:
    std::string command;
public:
    kshell_umount() : command("umount") {}
    const std::string &Command() const override {
        return command;
    }
    void Exec(kshell &, const std::vector<std::string> &cmd) override;
};


#endif //JEOKERNEL_KSHELL_UMOUNT_H
