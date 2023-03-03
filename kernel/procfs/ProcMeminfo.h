//
// Created by sigsegv on 3/3/23.
//

#ifndef JEOKERNEL_PROCMEMINFO_H
#define JEOKERNEL_PROCMEMINFO_H

#include "ProcSysLazyStrfile.h"

class ProcMeminfo : public ProcSysLazyStrfile {
public:
    std::string GetContent() override;
};


#endif //JEOKERNEL_PROCMEMINFO_H
