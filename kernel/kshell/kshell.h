//
// Created by sigsegv on 1/27/22.
//

#ifndef JEOKERNEL_KSHELL_H
#define JEOKERNEL_KSHELL_H

#include <thread>
#include <string>
#include <kfs/kfiles.h>

class kshell;

class kshell_command {
public:
    virtual ~kshell_command() {}
    virtual const std::string &Command() const = 0;
    virtual void Exec(kshell &shell, const std::vector<std::string> &cmd) = 0;
};

class kshell {
private:
    std::mutex mtx;
    bool exit;
    std::thread shell;
    std::vector<std::shared_ptr<kshell_command>> commands;
    std::shared_ptr<kfile> cwd_ref;
    kdirectory *cwd;
public:
    kshell();
    ~kshell();
    void Cwd(const std::shared_ptr<kfile> &cwd) {
        this->cwd_ref = cwd;
        this->cwd = dynamic_cast<kdirectory *> (&(*(this->cwd_ref)));
    }
    kdirectory &Cwd() {
        return *cwd;
    }
    std::shared_ptr<kfile> CwdRef() {
        return cwd_ref;
    }
    constexpr bool is_white(int ch) const {
        return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
    }
    std::vector<std::string> Parse(const std::string &cmd);
    void Exec(const std::vector<std::string> &cmd);

    void AddCommand(std::shared_ptr<kshell_command> command);

    [[noreturn]] void run();
};

#endif //JEOKERNEL_KSHELL_H
