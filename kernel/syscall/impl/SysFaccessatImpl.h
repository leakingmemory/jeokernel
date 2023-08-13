//
// Created by sigsegv on 8/13/23.
//

#ifndef JEOKERNEL_SYSFACCESSATIMPL_H
#define JEOKERNEL_SYSFACCESSATIMPL_H

#include <string>

class ProcThread;

class SysFaccessatImpl {
public:
    static int DoFaccessat(ProcThread &proc, int dfd, std::string filename, int mode, int flags);
};


#endif //JEOKERNEL_SYSFACCESSATIMPL_H
