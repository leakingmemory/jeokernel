//
// Created by sigsegv on 1/30/22.
//

#include "kshell_commands.h"
#include <sstream>
#include "kshell_ps.h"
#include <acpi/acpica_interface.h>

class kshell_echo : public kshell_command {
private:
    std::string command;
public:
    kshell_echo() : command("echo") {}
    const std::string &Command() const override {
        return command;
    }
    void Exec(const std::vector<std::string> &cmd) {
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
    void Exec(const std::vector<std::string> &cmd) {
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
    void Exec(const std::vector<std::string> &cmd) {
        get_acpica_interface().poweroff();
    }
};

kshell_commands::kshell_commands(kshell &shell) {
    shell.AddCommand(std::make_shared<kshell_echo>());
    shell.AddCommand(std::make_shared<kshell_reboot>());
    shell.AddCommand(std::make_shared<kshell_poweroff>());
    shell.AddCommand(std::make_shared<kshell_ps>());
}