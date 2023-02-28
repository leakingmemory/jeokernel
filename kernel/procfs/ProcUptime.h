//
// Created by sigsegv on 2/28/23.
//

#ifndef JEOKERNEL_PROCUPTIME_H
#define JEOKERNEL_PROCUPTIME_H

#include "ProcSysLazyStrfile.h"

class ProcUptime : public ProcSysLazyStrfile {
public:
    std::string GetContent() override;
};


#endif //JEOKERNEL_PROCUPTIME_H
