//
// Created by sigsegv on 10/4/22.
//

#ifndef JEOKERNEL_SYSCALLCTX_H
#define JEOKERNEL_SYSCALLCTX_H

#include <exec/callctx.h>
#include "SyscallHandler.h"

class SyscallCtx : public callctx {
public:
    explicit SyscallCtx(SyscallAdditionalParams &params);
};


#endif //JEOKERNEL_SYSCALLCTX_H
