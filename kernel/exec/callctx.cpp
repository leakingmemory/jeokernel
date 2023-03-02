//
// Created by sigsegv on 10/4/22.
//

#include <exec/callctx.h>
#include <exec/procthread.h>
#include <exec/usermem.h>
#include <errno.h>
#include <core/x86fpu.h>

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
    static intptr_t ReadString(std::shared_ptr<callctx_impl> ref, intptr_t ptr, std::function<resolve_return_value (const std::string &)>);
    static resolve_return_value NestedRead(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>);
    static resolve_return_value NestedWrite(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>);
    static resolve_return_value NestedReadNullterminatedArrayOfPointers(std::shared_ptr<callctx_impl> ref, intptr_t ptr, std::function<resolve_return_value (void **, size_t)>);
    static resolve_return_value NestedReadString(std::shared_ptr<callctx_impl> ref, intptr_t ptr, std::function<resolve_return_value (const std::string &)>);
    static resolve_return_value NestedReadArrayOfStrings(std::shared_ptr<callctx_impl> ref, void **vptr, size_t len, std::function<resolve_return_value (const std::vector<std::string> &)>);
    static void ReturnWhenNotRunning(std::shared_ptr<callctx_impl> ref, intptr_t value);
    static void KillAsync(std::shared_ptr<callctx_impl> ref);
    static void EntrypointAsync(std::shared_ptr<callctx_impl> ref, uintptr_t entrypoint, uintptr_t fsBase, uintptr_t stackPtr);
    static std::shared_ptr<callctx_async> AsyncCtx(std::shared_ptr<callctx_impl> ref);
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

intptr_t callctx_impl::ReadString(std::shared_ptr<callctx_impl> ref, intptr_t ptr,
                                  std::function<resolve_return_value(const std::string &)> func) {
    auto result = ref->process->resolve_read_nullterm(ptr, 1, ref->is_async, [ref] (intptr_t result) {
        ref->async->returnAsync(result);
    }, [ref, func, ptr] (bool success, bool async, size_t len, const std::function<void (uintptr_t)> &) mutable {
        ref->is_async = async;
        if (!success) {
            return resolve_return_value::Return(-EFAULT);
        }
        std::string str{};
        {
            UserMemory usermem{*(ref->process), (uintptr_t) ptr, ((uintptr_t) len)};
            if (!usermem) {
                return resolve_return_value::Return(-EFAULT);
            }
            str.append((const char *) usermem.Pointer(), len);
        }
        return func(str);
    });
    if (result.async) {
        ref->async->async();
        return 0;
    } else {
        return result.result;
    }
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

resolve_return_value callctx_impl::NestedReadNullterminatedArrayOfPointers(std::shared_ptr<callctx_impl> ref,
                                                                           intptr_t ptr,
                                                                           std::function<resolve_return_value(
                                                                                   void **, size_t len)> func) {
    auto result = ref->process->resolve_read_nullterm(ptr, sizeof(void *), ref->is_async, [ref] (intptr_t result) {
        ref->async->returnAsync(result);
    }, [ref, ptr, func] (bool success, bool async, size_t len, const std::function<void (uintptr_t)> &) mutable {
        ref->is_async |= async;
        if (!success) {
            return resolve_return_value::Return(-EFAULT);
        }
        std::shared_ptr<UserMemory> usermem{new UserMemory(*(ref->process), (uintptr_t) ptr, ((uintptr_t) len) * sizeof(void *))};
        ref->memRefs.push_back(usermem);
        if (!(*usermem)) {
            return resolve_return_value::Return(-EFAULT);
        }
        return func((void **) usermem->Pointer(), len);
    });
    if (result.hasValue) {
        return resolve_return_value::Return(result.result);
    } else if (result.async) {
        return resolve_return_value::AsyncReturn();
    } else {
        return resolve_return_value::NoReturn();
    }
}

resolve_return_value callctx_impl::NestedReadArrayOfStrings(std::shared_ptr<callctx_impl> ref, void **vptr, size_t len,
                                                            std::function<resolve_return_value(
                                                                    const std::vector<std::string> &)> func) {
    if (len <= 0) {
        std::vector<std::string> emptyArray{};
        return func(emptyArray);
    }
    struct Context {
        hw_spinlock lock{};
        std::vector<std::string> strings{};
        resolve_return_value result{resolve_return_value::Return(0)};
        size_t remaining;
        bool complete;
        bool async;
        bool returned;
    };
    std::shared_ptr<Context> context = std::make_shared<Context>();
    context->strings.reserve(len);
    context->remaining = len;
    context->complete = false;
    context->async = false;
    context->returned = false;
    for (size_t i = 0; i < len; i++) {
        context->strings.emplace_back();
    }
    for (size_t i = 0; i < len; i++) {
        intptr_t ptr = (intptr_t) vptr[i];
        ref->process->resolve_read_nullterm(ptr, 1, true, [ref, context] (intptr_t) {
            std::unique_lock lock{context->lock};
            if (context->complete && !context->returned) {
                auto result = context->result;
                context->returned = true;
                lock.release();
                if (result.hasValue) {
                    callctx_impl::ReturnWhenNotRunning(ref, result.result);
                }
            }
        }, [ref, context, func, ptr, i] (bool success, bool async, size_t len, const std::function<void(intptr_t)> &) mutable {
            if (!success) {
                std::lock_guard lock{context->lock};
                context->complete = true;
                context->result = resolve_return_value::Return(-EFAULT);
                return context->result;
            }
            std::string str{};
            {
                UserMemory userMemory{*(ref->process), (uintptr_t) ptr, len};
                if (!userMemory) {
                    std::lock_guard lock{context->lock};
                    context->complete = true;
                    context->result = resolve_return_value::Return(-EFAULT);
                    return context->result;
                }
                str.append((const char *) userMemory.Pointer(), len);
            }
            {
                std::lock_guard lock{context->lock};
                if (context->complete) {
                    return context->result;
                }
                context->strings[i] = str;
                --(context->remaining);
                if (context->remaining != 0) {
                    return resolve_return_value::Return(0);
                }
            }
            auto result = func(context->strings);
            std::lock_guard lock{context->lock};
            context->result = result;
            context->complete = true;
            return context->result;
        });
    }
    std::unique_lock lock{context->lock};
    if (context->complete && !context->returned) {
        auto result = context->result;
        context->returned = true;
        lock.release();
        return result;
    }
    context->async = true;
    return resolve_return_value::AsyncReturn();
}

void callctx_impl::ReturnWhenNotRunning(std::shared_ptr<callctx_impl> ref, intptr_t value) {
    ref->scheduler->when_not_running(*(ref->current_task), [ref, value] {
        ref->current_task->get_cpu_state().rax = value;
        ref->current_task->set_blocked(false);
    });
}

void callctx_impl::KillAsync(std::shared_ptr<callctx_impl> ref) {
    ref->scheduler->when_not_running(*(ref->current_task), [ref] {
        ref->current_task->set_end(true);
    });
}

void callctx_impl::EntrypointAsync(std::shared_ptr<callctx_impl> ref, uintptr_t entrypoint, uintptr_t fsBase,
                                   uintptr_t stackPtr) {
    ref->scheduler->when_not_running(*(ref->current_task), [ref, entrypoint, stackPtr, fsBase] {
        auto &fpu = get_fpu0_initial();
        uint32_t mxcsr = 0x1F80 & fpu.mxcsr_mask;
        ref->current_task->get_fpu_state() = {.fcw = fpu.fcw, .mxcsr = mxcsr, .mxcsr_mask = fpu.mxcsr_mask};
        auto &state = ref->current_task->get_cpu_state();
        auto &frame = ref->current_task->get_cpu_frame();
        state = {};
        frame = {};
        frame.rip = entrypoint;
        frame.cs = 0x18 | 3;
        frame.rsp = stackPtr;
        frame.ss = 0x20 | 3;
        state.ds = 0x20 | 3;
        state.es = 0x20 | 3;
        state.fs = 0x20 | 3;
        state.gs = 0x20 | 3;
        state.fsbase = fsBase;
        ref->current_task->set_blocked(false);
    });
}

std::shared_ptr<callctx_async> callctx_impl::AsyncCtx(std::shared_ptr<callctx_impl> ref) {
    return ref->async;
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

intptr_t callctx::ReadString(intptr_t ptr, std::function<resolve_return_value (const std::string &)> func) const {
    return callctx_impl::ReadString(impl, ptr, func);
}

resolve_return_value callctx::NestedRead(intptr_t ptr, intptr_t len, std::function<resolve_return_value(void *)> func) const {
    return callctx_impl::NestedRead(impl, ptr, len, func);
}

resolve_return_value callctx::NestedWrite(intptr_t ptr, intptr_t len, std::function<resolve_return_value(void *)> func) const {
    return callctx_impl::NestedWrite(impl, ptr, len, func);
}

resolve_return_value
callctx::NestedReadNullterminatedArrayOfPointers(intptr_t ptr, std::function<resolve_return_value(void **, size_t)> func) {
    return callctx_impl::NestedReadNullterminatedArrayOfPointers(impl, ptr, func);
}

resolve_return_value callctx::NestedReadArrayOfStrings(void **vptr, size_t len, std::function<resolve_return_value(
        const std::vector<std::string> &)> func) {
    return callctx_impl::NestedReadArrayOfStrings(impl, vptr, len, func);
}

void callctx::ReturnWhenNotRunning(intptr_t value) const {
    callctx_impl::ReturnWhenNotRunning(impl, value);
}

void callctx::KillAsync() const {
    callctx_impl::KillAsync(impl);
}

void callctx::EntrypointAsync(uintptr_t entrypoint, uintptr_t fsBase, uintptr_t stackPtr) const {
    callctx_impl::EntrypointAsync(impl, entrypoint, fsBase, stackPtr);
}

std::shared_ptr<callctx_async> callctx::AsyncCtx() const {
    return callctx_impl::AsyncCtx(impl);
}
