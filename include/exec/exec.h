//
// Created by sigsegv on 6/8/22.
//

#ifndef JEOKERNEL_EXEC_H
#define JEOKERNEL_EXEC_H

#include <memory>
#include <vector>
#include <string>
#include <sys/types.h>
#include <functional>
#include <resource/reference.h>
#include <resource/referrer.h>

class tty;
class kfile;
class kdirectory;

class ELF_loads;
class UserElf;
class exec_pageinfo;
class ProcThread;

class ExecState : public referrer {
private:
    std::weak_ptr<ExecState> selfRef{};
    reference<kfile> cwd_ref{};
    kdirectory &cwd;
    ExecState(kdirectory &cwd) : referrer("ExecState"), cwd(cwd) {}
public:
    static std::shared_ptr<ExecState> Create(const reference<kfile> &cwd_ref, kdirectory &cwd);
    std::string GetReferrerIdentifier() override;
    reference<kfile> ResolveFile(const std::shared_ptr<class referrer> &referrer, const std::string &filename);
};

class Process;

enum class ExecResult {
    DETACHED,
    SOFTERROR,
    FATALERROR
};

struct ExecStartVector {
    uintptr_t entrypoint;
    uintptr_t fsBase;
    uintptr_t stackAddr;
};

class Exec : public referrer {
private:
    std::weak_ptr<Exec> selfRef{};
    std::shared_ptr<class tty> tty;
    reference<kfile> cwd_ref{};
    kdirectory &cwd;
    reference<kfile> binary{};
    std::string name;
    std::vector<std::string> argv;
    std::vector<std::string> env;
    pid_t parent_pid;
    Exec(const std::shared_ptr<class tty> &tty, kdirectory &cwd, const std::string &name, const std::vector<std::string> &argv, const std::vector<std::string> &env, pid_t parent_pid) : referrer("Exec"), tty(tty), cwd(cwd), name(name), argv(argv), env(env), parent_pid(parent_pid) {}
public:
    Exec(const Exec &) = delete;
    Exec(Exec &&) = delete;
    Exec &operator =(const Exec &) = delete;
    Exec &operator =(Exec &&) = delete;
    static std::shared_ptr<Exec> Create(const std::shared_ptr<class tty> &tty, const reference<kfile> &cwd_ref, kdirectory &cwd, const reference<kfile> &binary, const std::string &name, const std::vector<std::string> &argv, const std::vector<std::string> &env, pid_t parent_pid);
    std::string GetReferrerIdentifier() override;
private:
    static bool LoadLoads(kfile &binary, ELF_loads &loads, UserElf &userElf);
    static void Pages(std::vector<exec_pageinfo> &pages, ELF_loads &loads, UserElf &userElf);
    static void MapPages(const reference<kfile> &binary, ProcThread *process, std::vector<exec_pageinfo> &pages, ELF_loads &loads, uintptr_t relocationOffset);
    static uintptr_t ProgramBreakAlign(uintptr_t pbrk);
    ExecResult Run(ProcThread *process, const std::function<void (bool success, const ExecStartVector &)> &);
public:
    ExecResult RunFromExistingProcess(ProcThread *process, const std::function<void (bool success, const ExecStartVector &)> &);
    std::shared_ptr<Process> Run(const std::function<void (const std::shared_ptr<Process> &)> &beforeStart = [] (const std::shared_ptr<Process> &) {});
};

#endif //JEOKERNEL_EXEC_H
