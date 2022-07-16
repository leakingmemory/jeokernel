//
// Created by sigsegv on 6/18/22.
//

#include <exec/procthread.h>
#include <exec/fdesc.h>
#include <errno.h>

void FileDescriptor::write(ProcThread *process, uintptr_t usersp_ptr, intptr_t len, std::function<void (intptr_t)> func) {
    auto handler = this->handler;
    process->resolve_read(usersp_ptr, len, [handler, usersp_ptr, len, func] (bool success) mutable {
        if (success) {
            func(handler->write((void *) usersp_ptr, len));
        } else {
            func(-EFAULT);
        }
    });
}
