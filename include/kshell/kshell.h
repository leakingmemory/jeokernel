//
// Created by sigsegv on 1/27/22.
//

#ifndef JEOKERNEL_KSHELL_H
#define JEOKERNEL_KSHELL_H

#include "thread"
#include "string"
#include "kfs/kfiles.h"
#include "resource/reference.h"
#include "resource/referrer.h"

class kshell;

class kshell_command {
public:
    virtual ~kshell_command() {}
    virtual const std::string &Command() const = 0;
    virtual void Exec(kshell &shell, const std::vector<std::string> &cmd) = 0;
};

class kshell : public referrer {
private:
    std::mutex mtx{};
    std::weak_ptr<kshell> selfRef{};
    std::vector<std::shared_ptr<kshell_command>> commands{};
    std::vector<std::function<void ()>> exitFuncs{};
    reference<kfile> cwd_ref{};
    kdirectory *cwd{nullptr};
    std::shared_ptr<class tty> tty;
    std::thread shell{};
    bool exit{false};
private:
    explicit kshell(const std::shared_ptr<class tty> &tty);
public:
    void Init(const std::shared_ptr<kshell> &selfRef);
    static std::shared_ptr<kshell> Create(const std::shared_ptr<class tty> &tty);
    std::string GetReferrerIdentifier() override;
    ~kshell();
    void OnExit(const std::function<void ()> &);
    void CwdRoot();
    void Cwd(const reference<kfile> &cwd);
    kdirectory &Cwd() {
        return *cwd;
    }
    reference<kfile> CwdRef(std::shared_ptr<class referrer> &referrer);
    std::shared_ptr<class tty> Tty() {
        return tty;
    }
    constexpr bool is_white(int ch) const {
        return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
    }
    std::vector<std::string> Parse(const std::string &cmd);
    void Exec(const std::vector<std::string> &cmd);

    void AddCommand(std::shared_ptr<kshell_command> command);

    void Exit() {
        exit = true;
    }

    void run();
};

#endif //JEOKERNEL_KSHELL_H
