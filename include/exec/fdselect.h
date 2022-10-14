//
// Created by sigsegv on 10/13/22.
//

#ifndef JEOKERNEL_FDSELECT_H
#define JEOKERNEL_FDSELECT_H

#include <memory>

struct fdset;
class SelectImpl;
class SyscallCtx;

class Select {
    std::shared_ptr<SelectImpl> impl;
public:
    Select(const SyscallCtx &ctx, int n, fdset *inp, fdset *outp, fdset *exc);
    explicit Select(const std::shared_ptr<SelectImpl> &impl) : impl(impl) {
    }
    bool KeepSubscription(int fd);
    void NotifyRead(int fd);
};

#endif //JEOKERNEL_FDSELECT_H
