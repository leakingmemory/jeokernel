//
// Created by sigsegv on 6/8/22.
//

#ifndef JEOKERNEL_EXEC_H
#define JEOKERNEL_EXEC_H

#include <memory>
#include <vector>

class kfile;

class ELF_loads;
class UserElf;
class exec_pageinfo;
class ProcThread;

class Exec {
private:
    std::shared_ptr<kfile> binary;
    std::string name;
public:
    Exec(std::shared_ptr<kfile> binary, const std::string &name) : binary(binary), name(name) {}
private:
    bool LoadLoads(ELF_loads &loads, UserElf &userElf);
    void Pages(std::vector<exec_pageinfo> &pages, ELF_loads &loads, UserElf &userElf);
    void MapPages(ProcThread *process, std::vector<exec_pageinfo> &pages, ELF_loads &loads);
public:
    void Run();
};

#endif //JEOKERNEL_EXEC_H
