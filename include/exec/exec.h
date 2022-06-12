//
// Created by sigsegv on 6/8/22.
//

#ifndef JEOKERNEL_EXEC_H
#define JEOKERNEL_EXEC_H

#include <memory>

class kfile;

class Exec {
private:
    std::shared_ptr<kfile> binary;
    std::string name;
public:
    Exec(std::shared_ptr<kfile> binary, const std::string &name) : binary(binary), name(name) {}
    void Run();
};

#endif //JEOKERNEL_EXEC_H
