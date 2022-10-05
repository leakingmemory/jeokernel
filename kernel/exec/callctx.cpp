//
// Created by sigsegv on 10/4/22.
//

#include <exec/callctx.h>
#include <exec/procthread.h>
#include <exec/usermem.h>
#include <errno.h>

class callctx_impl {
private:
    std::shared_ptr<callctx_async> async;
    tasklist *scheduler;
    task *current_task;
    ProcThread *process;
    bool is_async;
public:
    callctx_impl(std::shared_ptr<callctx_async> async);
    static intptr_t Resolve(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value ()>);
    static intptr_t Read(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>);
    static intptr_t Write(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>);
};

callctx_impl::callctx_impl(std::shared_ptr<callctx_async> async) :
async(async), scheduler(get_scheduler()), current_task(&(scheduler->get_current_task())),
process(scheduler->get_resource<ProcThread>(*current_task)) {
}

intptr_t callctx_impl::Resolve(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value()> func) {
    auto result = ref->process->resolve_read(ptr, len, ref->is_async, [ref] (intptr_t result) {
        ref->async->returnAsync(result);
    }, [ref, func] (bool success, bool, const std::function<void (uintptr_t)> &) mutable {
        if (!success) {
            return resolve_return_value::Return(-EFAULT);
        }
        return func();
    });
    if (result.async) {
        ref->async->async();
        return 0;
    } else {
        return result.result;
    }
}

intptr_t callctx_impl::Read(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value(void *)> func) {
    return Resolve(ref, ptr, len, [ref, ptr, len, func] () mutable {
        UserMemory usermem{*(ref->process), (uintptr_t) ptr, (uintptr_t) len};
        if (!usermem) {
            return resolve_return_value::Return(-EFAULT);
        }
        return func(usermem.Pointer());
    });
}

intptr_t callctx_impl::Write(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value(void *)> func) {
    return Resolve(ref, ptr, len, [ref, ptr, len, func] () mutable {
        if (!ref->process->resolve_write(ptr, len)) {
            return resolve_return_value::Return(-EFAULT);;
        }
        UserMemory usermem{*(ref->process), (uintptr_t) ptr, (uintptr_t) len, true};
        if (!usermem) {
            return resolve_return_value::Return(-EFAULT);
        }
        return func(usermem.Pointer());
    });
}

callctx::callctx(std::shared_ptr<callctx_async> async) : impl(new callctx_impl(async)) {
}

intptr_t callctx::Read(intptr_t ptr, intptr_t len, std::function<resolve_return_value(void *)> func) {
    return callctx_impl::Read(impl, ptr, len, func);
}

intptr_t callctx::Write(intptr_t ptr, intptr_t len, std::function<resolve_return_value(void *)> func) {
    return callctx_impl::Write(impl, ptr, len, func);
}