//
// Created by sigsegv on 7/27/22.
//

#ifndef JEOKERNEL_USERMEM_H
#define JEOKERNEL_USERMEM_H

#include <memory>

class ProcThread;
class vmem;

class UserMemory {
private:
    std::shared_ptr<vmem> vm;
    uintptr_t offset;
    bool valid;
public:
    UserMemory(ProcThread &proc, uintptr_t uptr, uintptr_t len, bool write=false);
    void *Pointer() const;
    operator bool () {
        return valid;
    }
};

#endif //JEOKERNEL_USERMEM_H
