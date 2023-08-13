//
// Created by sigsegv on 8/13/23.
//

#ifndef JEOKERNEL_SYSMEMBARRIERIMPL_H
#define JEOKERNEL_SYSMEMBARRIERIMPL_H

class ProcThread;

class SysMembarrierImpl {
public:
    static int DoMembarrier(ProcThread &process, int cmd, unsigned int flags, int cpu_id);
};


#endif //JEOKERNEL_SYSMEMBARRIERIMPL_H
