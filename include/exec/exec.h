//
// Created by sigsegv on 6/8/22.
//

#ifndef JEOKERNEL_EXEC_H
#define JEOKERNEL_EXEC_H

#include <memory>
#include <vector>
#include <string>

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

class Exec {
private:
    std::shared_ptr<kfile> cwd_ref;
    kdirectory &cwd;
    std::shared_ptr<kfile> binary;
    std::string name;
public:
    Exec(const std::shared_ptr<kfile> &cwd_ref, kdirectory &cwd, std::shared_ptr<kfile> binary, const std::string &name) : cwd_ref(cwd_ref), cwd(cwd), binary(binary), name(name) {}
private:
    static bool LoadLoads(kfile &binary, ELF_loads &loads, UserElf &userElf);
    static void Pages(std::vector<exec_pageinfo> &pages, ELF_loads &loads, UserElf &userElf);
    static void MapPages(std::shared_ptr<kfile> binary, ProcThread *process, std::vector<exec_pageinfo> &pages, ELF_loads &loads, uintptr_t relocationOffset);
public:
    void Run();
};

#endif //JEOKERNEL_EXEC_H
