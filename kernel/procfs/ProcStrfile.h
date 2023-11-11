//
// Created by sigsegv on 2/24/23.
//

#ifndef JEOKERNEL_PROCSTRFILE_H
#define JEOKERNEL_PROCSTRFILE_H

#include "ProcDatafile.h"
#include <string>

class ProcStrfile : public ProcDatafile {
private:
    std::string str;
protected:
    ProcStrfile(fsresourcelockfactory &lockfactory, const std::string &str) : ProcDatafile(lockfactory), str(str) {}
public:
    static std::shared_ptr<ProcStrfile> Create(const std::string &str);
    std::size_t Size() override;
    file_read_result Read(uint64_t offset, void *ptr, std::size_t length) override;
};


#endif //JEOKERNEL_PROCSTRFILE_H
