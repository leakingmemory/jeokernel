//
// Created by sigsegv on 1/27/22.
//

#ifndef JEOKERNEL_KSHELL_H
#define JEOKERNEL_KSHELL_H

#include <thread>
#include <string>

class kshell_command {
public:
    virtual ~kshell_command() {}
    virtual const std::string &Command() const = 0;
    virtual void Exec(const std::vector<std::string> &cmd) = 0;
};

class kshell {
private:
    std::mutex mtx;
    bool exit;
    std::thread shell;
    std::vector<std::shared_ptr<kshell_command>> commands;
public:
    kshell();
    ~kshell();
    constexpr bool is_white(int ch) const {
        return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
    }
    std::vector<std::string> Parse(const std::string &cmd);
    void Exec(const std::vector<std::string> &cmd);

    void AddCommand(std::shared_ptr<kshell_command> command);

    [[noreturn]] void run();
};

#endif //JEOKERNEL_KSHELL_H
