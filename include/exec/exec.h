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

class tty;
class kfile;
class kdirectory;

class ELF_loads;
class UserElf;
class exec_pageinfo;
class ProcThread;

class ExecState {
private:
    std::shared_ptr<kfile> cwd_ref;
    kdirectory &cwd;
public:
    ExecState(const std::shared_ptr<kfile> &cwd_ref, kdirectory &cwd) : cwd_ref(cwd_ref), cwd(cwd) {}
    std::shared_ptr<kfile> ResolveFile(const std::string &filename);
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

class Exec {
private:
    std::shared_ptr<class tty> tty;
    std::shared_ptr<kfile> cwd_ref;
    kdirectory &cwd;
    std::shared_ptr<kfile> binary;
    std::string name;
    std::vector<std::string> argv;
    std::vector<std::string> env;
    pid_t parent_pid;
public:
    Exec(const std::shared_ptr<class tty> &tty, const std::shared_ptr<kfile> &cwd_ref, kdirectory &cwd, std::shared_ptr<kfile> binary, const std::string &name, const std::vector<std::string> &argv, const std::vector<std::string> &env, pid_t parent_pid) : tty(tty), cwd_ref(cwd_ref), cwd(cwd), binary(binary), name(name), argv(argv), env(env), parent_pid(parent_pid) {}
private:
    static bool LoadLoads(kfile &binary, ELF_loads &loads, UserElf &userElf);
    static void Pages(std::vector<exec_pageinfo> &pages, ELF_loads &loads, UserElf &userElf);
    static void MapPages(std::shared_ptr<kfile> binary, ProcThread *process, std::vector<exec_pageinfo> &pages, ELF_loads &loads, uintptr_t relocationOffset);
public:
    ExecResult Run(ProcThread *process, const std::function<void (bool success, const ExecStartVector &)> &);
    std::shared_ptr<Process> Run();
};

#endif //JEOKERNEL_EXEC_H
