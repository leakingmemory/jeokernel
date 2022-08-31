//
// Created by sigsegv on 6/8/22.
//

#ifndef JEOKERNEL_EXEC_H
#define JEOKERNEL_EXEC_H

#include <memory>

class kfile;

class ELF_loads;
class UserElf;

class Exec {
private:
    std::shared_ptr<kfile> binary;
    std::string name;
public:
    Exec(std::shared_ptr<kfile> binary, const std::string &name) : binary(binary), name(name) {}
private:
    void LoadLoads(ELF_loads &loads, UserElf &userElf);
public:
    void Run();
};

#endif //JEOKERNEL_EXEC_H
