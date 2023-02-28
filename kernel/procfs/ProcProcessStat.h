//
// Created by sigsegv on 2/27/23.
//

#ifndef JEOKERNEL_PROCPROCESSSTAT_H
#define JEOKERNEL_PROCPROCESSSTAT_H

#include "ProcLazyStrfile.h"

class ProcProcessStat : public ProcLazyStrfile {
public:
    ProcProcessStat(const std::shared_ptr<Process> &process) : ProcLazyStrfile(process) {}
    std::string GetContent(Process &proc) override;
};


#endif //JEOKERNEL_PROCPROCESSSTAT_H
