//
// Created by sigsegv on 3/4/23.
//

#ifndef JEOKERNEL_PROCMOUNTS_H
#define JEOKERNEL_PROCMOUNTS_H

#include "ProcSysLazyStrfile.h"

class ProcMounts : public ProcSysLazyStrfile {
private:
    ProcMounts(fsresourcelockfactory &lockfactory) : ProcSysLazyStrfile(lockfactory) {}
public:
    ProcMounts() = delete;
    static std::shared_ptr<ProcMounts> Create();
    std::string GetContent() override;
};


#endif //JEOKERNEL_PROCMOUNTS_H
