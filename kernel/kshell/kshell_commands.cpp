//
// Created by sigsegv on 1/30/22.
//

#include <kshell/kshell_commands.h>
#include <sstream>
#include <iostream>
#include "kshell_ps.h"
#include "kshell_stats.h"
#include "kshell_blockdevices.h"
#include "kshell_ls.h"
#include "kshell_mount.h"
#include "kshell_cd.h"
#include "kshell_cat.h"
#include "kshell_exec.h"
#include "kshell_umount.h"
#include <acpi/acpica_interface.h>

class kshell_echo : public kshell_command {
private:
    std::string command;
public:
    kshell_echo() : command("echo") {}
    const std::string &Command() const override {
        return command;
    }
    void Exec(kshell &, const std::vector<std::string> &cmd) override {
        auto iterator = cmd.cbegin();
        if (iterator == cmd.cend()) {
            get_klogger() << "\n";
            return;
        }
        ++iterator;
        std::stringstream str{};
        if (iterator != cmd.cend()) {
            str << *iterator;
            ++iterator;
            while (iterator != cmd.cend()) {
                str << " " << *iterator;
                ++iterator;
            }
        }
        str << "\n";
        get_klogger() << str.str().c_str();
    }
};

class kshell_reboot : public kshell_command {
private:
    std::string command;
public:
    kshell_reboot() : command("reboot") {}
    const std::string &Command() const override {
        return command;
    }
    void Exec(kshell &, const std::vector<std::string> &cmd) override {
        get_acpica_interface().reboot();
    }
};

class kshell_poweroff : public kshell_command {
private:
    std::string command;
public:
    kshell_poweroff() : command("poweroff") {}
    const std::string &Command() const override {
        return command;
    }
    void Exec(kshell &, const std::vector<std::string> &cmd) override {
        get_acpica_interface().poweroff();
    }
};

class kshell_pwd : public kshell_command {
private:
    std::string command;
public:
    kshell_pwd() : command("pwd") {}
    const std::string &Command() const override {
        return command;
    }
    void Exec(kshell &shell, const std::vector<std::string> &cmd) override {
        auto &cwd = shell.Cwd();
        std::cout << cwd.Kpath() << "\n";
    }
};

class kshell_exit : public kshell_command {
private:
    std::string command;
public:
    kshell_exit() : command("exit") {}
    const std::string &Command() const override {
        return command;
    }
    void Exec(kshell &shell, const std::vector<std::string> &cmd) override {
        shell.Exit();
    }
};

kshell_commands::kshell_commands(kshell &shell) {
    shell.AddCommand(std::make_shared<kshell_echo>());
    shell.AddCommand(std::make_shared<kshell_reboot>());
    shell.AddCommand(std::make_shared<kshell_poweroff>());
    shell.AddCommand(std::make_shared<kshell_pwd>());
    shell.AddCommand(std::make_shared<kshell_exit>());
    shell.AddCommand(std::make_shared<kshell_ps>());
    shell.AddCommand(std::make_shared<kshell_stats>());
    shell.AddCommand(std::make_shared<kshell_blockdevices>());
    shell.AddCommand(std::make_shared<kshell_ls>());
    shell.AddCommand(std::make_shared<kshell_mount>());
    shell.AddCommand(std::make_shared<kshell_cd>());
    shell.AddCommand(std::make_shared<kshell_cat>());
    shell.AddCommand(std::make_shared<kshell_exec>());
    shell.AddCommand(std::make_shared<kshell_umount>());
}
