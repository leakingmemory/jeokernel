//
// Created by sigsegv on 2/27/23.
//

#ifndef JEOKERNEL_PROCPROCESSSTAT_H
#define JEOKERNEL_PROCPROCESSSTAT_H

#include "ProcLazyStrfile.h"

class ProcProcessStat : public ProcLazyStrfile {
private:
    ProcProcessStat(fsresourcelockfactory &lockfactory, const std::shared_ptr<Process> &process) : ProcLazyStrfile(lockfactory, process) {}
public:
    static std::shared_ptr<ProcProcessStat> Create(const std::shared_ptr<Process> &process);
    std::string GetContent(Process &proc) override;
};


#endif //JEOKERNEL_PROCPROCESSSTAT_H
