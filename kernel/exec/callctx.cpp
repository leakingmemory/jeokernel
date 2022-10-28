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
    std::vector<std::shared_ptr<UserMemory>> memRefs;
    tasklist *scheduler;
    task *current_task;
    ProcThread *process;
    bool is_async;
public:
    callctx_impl(std::shared_ptr<callctx_async> async);
    ProcThread &GetProcess() const;
    static intptr_t Await(std::shared_ptr<callctx_impl> ref, resolve_return_value returnValue);
    static intptr_t Resolve(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value ()>);
    static resolve_return_value NestedResolve(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value ()>);
    static intptr_t Read(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>);
    static intptr_t Write(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>);
    static resolve_return_value NestedRead(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>);
    static resolve_return_value NestedWrite(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>);
    static void ReturnWhenNotRunning(std::shared_ptr<callctx_impl> ref, intptr_t value);
};

callctx_impl::callctx_impl(std::shared_ptr<callctx_async> async) :
async(async), memRefs(), scheduler(get_scheduler()), current_task(&(scheduler->get_current_task())),
process(scheduler->get_resource<ProcThread>(*current_task)) {
}

ProcThread &callctx_impl::GetProcess() const {
    return *process;
}

intptr_t callctx_impl::Await(std::shared_ptr<callctx_impl> ref, resolve_return_value returnValue) {
    if (returnValue.async) {
        ref->async->async();
        return 0;
    } else {
        return returnValue.result;
    }
}

intptr_t callctx_impl::Resolve(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value()> func) {
    auto result = ref->process->resolve_read(ptr, len, ref->is_async, [ref] (intptr_t result) {
        ref->async->returnAsync(result);
    }, [ref, func] (bool success, bool async, const std::function<void (uintptr_t)> &) mutable {
        ref->is_async = async;
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

resolve_return_value callctx_impl::NestedResolve(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value()> func) {
    auto result = ref->process->resolve_read(ptr, len, ref->is_async, [ref] (intptr_t result) {
        ref->async->returnAsync(result);
    }, [ref, func] (bool success, bool async, const std::function<void (uintptr_t)> &) mutable {
        ref->is_async |= async;
        if (!success) {
            return resolve_return_value::Return(-EFAULT);
        }
        return func();
    });
    if (result.hasValue) {
        return resolve_return_value::Return(result.result);
    } else if (result.async) {
        return resolve_return_value::AsyncReturn();
    } else {
        return resolve_return_value::NoReturn();
    }
}

intptr_t callctx_impl::Read(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value(void *)> func) {
    return Resolve(ref, ptr, len, [ref, ptr, len, func] () mutable {
        std::shared_ptr<UserMemory> usermem{new UserMemory(*(ref->process), (uintptr_t) ptr, (uintptr_t) len)};
        ref->memRefs.push_back(usermem);
        if (!(*usermem)) {
            return resolve_return_value::Return(-EFAULT);
        }
        return func(usermem->Pointer());
    });
}

intptr_t callctx_impl::Write(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value(void *)> func) {
    return Resolve(ref, ptr, len, [ref, ptr, len, func] () mutable {
        if (!ref->process->resolve_write(ptr, len)) {
            return resolve_return_value::Return(-EFAULT);;
        }
        std::shared_ptr<UserMemory> usermem{new UserMemory(*(ref->process), (uintptr_t) ptr, (uintptr_t) len, true)};
        ref->memRefs.push_back(usermem);
        if (!(*usermem)) {
            return resolve_return_value::Return(-EFAULT);
        }
        return func(usermem->Pointer());
    });
}

resolve_return_value callctx_impl::NestedRead(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value(void *)> func) {
    return NestedResolve(ref, ptr, len, [ref, ptr, len, func] () mutable {
        std::shared_ptr<UserMemory> usermem{new UserMemory(*(ref->process), (uintptr_t) ptr, (uintptr_t) len)};
        ref->memRefs.push_back(usermem);
        if (!(*usermem)) {
            return resolve_return_value::Return(-EFAULT);
        }
        return func(usermem->Pointer());
    });
}

resolve_return_value callctx_impl::NestedWrite(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value(void *)> func) {
    return NestedResolve(ref, ptr, len, [ref, ptr, len, func] () mutable {
        if (!ref->process->resolve_write(ptr, len)) {
            return resolve_return_value::Return(-EFAULT);;
        }
        std::shared_ptr<UserMemory> usermem{new UserMemory(*(ref->process), (uintptr_t) ptr, (uintptr_t) len, true)};
        ref->memRefs.push_back(usermem);
        if (!(*usermem)) {
            return resolve_return_value::Return(-EFAULT);
        }
        return func(usermem->Pointer());
    });
}

void callctx_impl::ReturnWhenNotRunning(std::shared_ptr<callctx_impl> ref, intptr_t value) {
    ref->scheduler->when_not_running(*(ref->current_task), [ref, value] {
        ref->current_task->get_cpu_state().rax = value;
        ref->current_task->set_blocked(false);
    });
}

callctx::callctx(std::shared_ptr<callctx_async> async) : impl(new callctx_impl(async)) {
}

ProcThread &callctx::GetProcess() const {
    return impl->GetProcess();
}

resolve_return_value callctx::Async() const {
    return resolve_return_value::AsyncReturn();
}

intptr_t callctx::Await(resolve_return_value returnValue) const {
    return callctx_impl::Await(impl, returnValue);
}

intptr_t callctx::Read(intptr_t ptr, intptr_t len, std::function<resolve_return_value(void *)> func) const {
    return callctx_impl::Read(impl, ptr, len, func);
}

intptr_t callctx::Write(intptr_t ptr, intptr_t len, std::function<resolve_return_value(void *)> func) const {
    return callctx_impl::Write(impl, ptr, len, func);
}

resolve_return_value callctx::NestedRead(intptr_t ptr, intptr_t len, std::function<resolve_return_value(void *)> func) const {
    return callctx_impl::NestedRead(impl, ptr, len, func);
}

resolve_return_value callctx::NestedWrite(intptr_t ptr, intptr_t len, std::function<resolve_return_value(void *)> func) const {
    return callctx_impl::NestedWrite(impl, ptr, len, func);
}

void callctx::ReturnWhenNotRunning(intptr_t value) const {
    callctx_impl::ReturnWhenNotRunning(impl, value);
}
