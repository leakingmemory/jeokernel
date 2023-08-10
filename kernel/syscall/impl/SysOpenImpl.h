//
// Created by sigsegv on 8/10/23.
//

#ifndef JEOKERNEL_SYSOPENIMPL_H
#define JEOKERNEL_SYSOPENIMPL_H

#include <string>

class ProcThread;

class SysOpenImpl {
public:
    static int DoOpenAt(ProcThread &, int dfd, const std::string &filename, int flags, int mode);
};


#endif //JEOKERNEL_SYSOPENIMPL_H
