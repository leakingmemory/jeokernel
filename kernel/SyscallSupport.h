//
// Created by sigsegv on 6/15/22.
//

#ifndef JEOKERNEL_SYSCALLSUPPORT_H
#define JEOKERNEL_SYSCALLSUPPORT_H


class SyscallSupport {
private:
    SyscallSupport();
public:
    static SyscallSupport &Instance();
    SyscallSupport &CpuSetup();
    SyscallSupport &GlobalSetup();
};


#endif //JEOKERNEL_SYSCALLSUPPORT_H
