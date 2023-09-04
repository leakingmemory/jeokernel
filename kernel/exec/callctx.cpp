//
// Created by sigsegv on 10/4/22.
//

#include <exec/callctx.h>
#include <exec/procthread.h>
#include <exec/usermem.h>
#include <errno.h>
#include <core/x86fpu.h>
#include <iostream>
#include <interrupt_frame.h>

//#define DEBUG_LAUNCH_SIGNAL

static void LaunchSignal(struct sigaction sigaction, tasklist *scheduler, InterruptCpuFrame &cpuframe, InterruptStackFrame &cpustate, task *t, ProcThread *procthread, int signum) {
    uintptr_t oldmask = 0;
    {
        sigset_t sig{};
        sig.Set(signum);
        procthread->sigprocmask(SIG_BLOCK, &sig, &sig, sizeof(sig));
        memcpy((void *) &oldmask, (void *) &sig, sizeof(oldmask) < sizeof(sig) ? sizeof(oldmask) : sizeof(sig));
    }
    uintptr_t sigtramp{(sigaction.sa_flags & SA_RESTORER) != 0 ? (uintptr_t) sigaction.sa_restorer : SIGTRAMP_ADDR};
#ifdef DEBUG_LAUNCH_SIGNAL
    std::cout << "Signal " << std::dec << signum << ", trampoline " << std::hex << sigtramp << "\n";
#endif
    SignalStackFrame stackFrame{
            .sigtramp = sigtramp,
            .r8 = cpustate.r8,
            .r9 = cpustate.r9,
            .r10 = cpustate.r10,
            .r11 = cpustate.r11,
            .r12 = cpustate.r12,
            .r13 = cpustate.r13,
            .r14 = cpustate.r14,
            .r15 = cpustate.r15,
            .rdi = cpustate.rdi,
            .rsi = cpustate.rsi,
            .rbp = cpustate.rbp,
            .rbx = cpustate.rbx,
            .rdx = cpustate.rdx,
            .rax = cpustate.rax,
            .rcx = cpustate.rcx,
            .rsp = cpuframe.rsp,
            .rip = cpuframe.rip,
            .rflags = cpuframe.rflags,
            .cs = (uint16_t) cpuframe.cs,
            .gs = cpustate.gs,
            .fs = cpustate.fs,
            .err = 0,
            .trapno = 0,
            .oldmask = oldmask,
            .cr2 = 0,
            .fpstate = 0
    };
    auto frameptr = cpuframe.rsp - 128 - sizeof(stackFrame);
    frameptr &= ~((typeof(frameptr)) 0xF);
    frameptr -= sizeof(uintptr_t);
    procthread->resolve_read(frameptr, sizeof(stackFrame), false, [] (intptr_t) {}, [scheduler, t, procthread, sigaction, stackFrame, frameptr, signum] (bool success, bool async, const std::function<void (uintptr_t)> &) {
        if (success && procthread->resolve_write(frameptr, sizeof(stackFrame))) {
            UserMemory umem{*procthread, frameptr, sizeof(stackFrame), true};
            if (umem) {
                uintptr_t sa_handler;
                if ((sigaction.sa_flags & SA_SIGINFO) != 0) {
                    sa_handler = (uintptr_t) sigaction.sa_sigaction;
                } else {
                    sa_handler = (uintptr_t) sigaction.sa_handler;
                }
#ifdef DEBUG_LAUNCH_SIGNAL
                std::cout << "Launching signal handler " << std::hex << sa_handler << " with frame at " << frameptr << " with flags " << sigaction.sa_flags << std::dec << "\n";
#endif
                memcpy((typeof(stackFrame) *) umem.Pointer(), &stackFrame, sizeof(stackFrame));
                scheduler->when_not_running(*t, [t, sa_handler, frameptr, signum] () {
                    auto &cpuframe = t->get_cpu_frame();
                    auto &cpustate = t->get_cpu_state();
                    cpuframe.rip = sa_handler;
                    cpuframe.rsp = frameptr;
                    cpustate.rdi = signum;
                    t->set_blocked(false);
                });
            } else {
                scheduler->when_not_running(*t, [scheduler, t] () {
                    scheduler->evict_task_with_lock(*t);
                });
            }
        } else {
            scheduler->when_not_running(*t, [scheduler, t] () {
                scheduler->evict_task_with_lock(*t);
            });
        }
        return resolve_return_value::AsyncReturn();
    });
}

void callctx_async::HandleSignalInWhenNotRunning(tasklist *scheduler, task *t, ProcThread *procthread, struct sigaction sigaction, int signum) {
    if (signum == SIGKILL) {
        scheduler->evict_task_with_lock(*t);
        scheduler->when_out_of_lock([] () {
            std::cerr << "Killed\n";
        });
        return;
    }
    if (sigaction.sa_handler == SIG_DFL)
    {
        auto tid = procthread->gettid();
        Interrupt intr{&(t->get_cpu_frame()), &(t->get_cpu_state()), &(t->get_fpu_state()), 0x80};
        scheduler->evict_task_with_lock(*t);
        scheduler->when_out_of_lock([intr, signum, tid] () {
            std::cerr << "Signal " << signum << "A tid=" << tid << "\n";
            intr.print_debug();
        });
        return;
    }
    scheduler->when_out_of_lock([scheduler, t, procthread, sigaction, signum] () {
        auto &cpuframe = t->get_cpu_frame();
        auto &cpustate = t->get_cpu_state();
        LaunchSignal(sigaction, scheduler, cpuframe, cpustate, t, procthread, signum);
    });
}

bool callctx::HandleSignalInFastReturn(Interrupt &intr) {
    auto *scheduler = get_scheduler();
    auto &task = scheduler->get_current_task();
    auto *pt = task.get_resource<ProcThread>();
    if (pt == nullptr) {
        return false;
    }
    int sig = pt->GetAndClearSigpending();
    if (sig <= 0) {
        return false;
    }
    task.set_blocked(true);
    auto &cpuframe = intr.get_cpu_frame();
    auto &cpustate = intr.get_cpu_state();
    auto optSigaction = pt->GetSigaction(sig);
    if (!optSigaction || optSigaction->sa_handler == SIG_DFL) {
        task.set_end(true);
        if (sig == SIGKILL) {
            std::cerr << "Killed\n";
        } else {
            std::cerr << "Signal " << sig << " FR tid=" << pt->gettid() << "\n";
            intr.print_debug();
        }
        return true;
    }
    LaunchSignal(*optSigaction, scheduler, cpuframe, cpustate, &task, pt, sig);
    return true;
}

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
    callctx_impl(const callctx_impl &);
    callctx_impl(callctx_impl &&) = delete;
    callctx_impl &operator =(const callctx_impl &) = delete;
    callctx_impl &operator =(callctx_impl &&) = delete;
    ProcThread &GetProcess() const;
    static void Aborter(std::shared_ptr<callctx_impl> ref, const std::function<void ()> &);
    static intptr_t Await(std::shared_ptr<callctx_impl> ref, resolve_return_value returnValue);
    static intptr_t Resolve(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value ()>);
    static resolve_return_value NestedResolve(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value ()>, std::function<resolve_return_value ()> fault);
    static resolve_return_value NestedResolve(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value ()>);
    static intptr_t Read(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>);
    static intptr_t Write(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>);
    static intptr_t ReadString(std::shared_ptr<callctx_impl> ref, intptr_t ptr, std::function<resolve_return_value (const std::string &)>);
    static resolve_return_value NestedRead(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>);
    static resolve_return_value NestedWrite(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>);
    static resolve_return_value NestedWrite(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>, std::function<resolve_return_value ()> fault);
    static resolve_return_value NestedReadNullterminatedArrayOfPointers(std::shared_ptr<callctx_impl> ref, intptr_t ptr, std::function<resolve_return_value (void **, size_t)>);
    static resolve_return_value NestedReadString(std::shared_ptr<callctx_impl> ref, intptr_t ptr, std::function<resolve_return_value (const std::string &)>);
    static resolve_return_value NestedReadArrayOfStrings(std::shared_ptr<callctx_impl> ref, void **vptr, size_t len, std::function<resolve_return_value (const std::vector<std::string> &)>);
    static void ReturnWhenNotRunning(std::shared_ptr<callctx_impl> ref, intptr_t value);
    static void KillAsync(std::shared_ptr<callctx_impl> ref);
    static void EntrypointAsync(std::shared_ptr<callctx_impl> ref, uintptr_t entrypoint, uintptr_t fsBase, uintptr_t stackPtr);
    static std::shared_ptr<callctx_async> AsyncCtx(std::shared_ptr<callctx_impl> ref);
    static std::shared_ptr<callctx_impl> ReturnFilter(std::shared_ptr<callctx_impl> impl, std::function<resolve_return_value(resolve_return_value)>);
};

callctx_impl::callctx_impl(std::shared_ptr<callctx_async> async) :
async(async), memRefs(), scheduler(get_scheduler()), current_task(&(scheduler->get_current_task())),
process(scheduler->get_resource<ProcThread>(*current_task)) {
}

callctx_impl::callctx_impl(const callctx_impl &cp) : async(cp.async), memRefs(cp.memRefs), scheduler(cp.scheduler), current_task(cp.current_task), process(cp.process), is_async(cp.is_async) {
}

ProcThread &callctx_impl::GetProcess() const {
    return *process;
}

void callctx_impl::Aborter(std::shared_ptr<callctx_impl> ref, const std::function<void()> &func) {
    ref->process->AborterFunc(func);
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

resolve_return_value callctx_impl::NestedResolve(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value()> func, std::function<resolve_return_value()> fault) {
    auto result = ref->process->resolve_read(ptr, len, ref->is_async, [ref] (intptr_t result) {
        ref->async->returnAsync(result);
    }, [ref, func, fault] (bool success, bool async, const std::function<void (uintptr_t)> &) mutable {
        ref->is_async |= async;
        if (!success) {
            return fault();
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

resolve_return_value callctx_impl::NestedResolve(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value()> func) {
    return NestedResolve(ref, ptr, len, func, [] () {
        return resolve_return_value::Return(-EFAULT);
    });
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

resolve_return_value callctx_impl::NestedWrite(std::shared_ptr<callctx_impl> ref, intptr_t ptr, intptr_t len, std::function<resolve_return_value(void *)> func, std::function<resolve_return_value()> fault) {
    return NestedResolve(ref, ptr, len, [ref, ptr, len, func, fault] () mutable {
        if (!ref->process->resolve_write(ptr, len)) {
            return fault();
        }
        std::shared_ptr<UserMemory> usermem{new UserMemory(*(ref->process), (uintptr_t) ptr, (uintptr_t) len, true)};
        ref->memRefs.push_back(usermem);
        if (!(*usermem)) {
            return fault();
        }
        return func(usermem->Pointer());
    }, fault);
}

resolve_return_value callctx_impl::NestedReadString(std::shared_ptr<callctx_impl> ref, intptr_t ptr,
                                                    std::function<resolve_return_value(const std::string &)> func) {
    auto result = ref->process->resolve_read_nullterm(ptr, 1, ref->is_async, [ref] (intptr_t result) {
        ref->async->returnAsync(result);
    }, [ref, func, ptr] (bool success, bool async, size_t len, const std::function<void (uintptr_t)> &) mutable {
        ref->is_async |= async;
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
    if (result.hasValue) {
        return resolve_return_value::Return(result.result);
    } else if (result.async) {
        return resolve_return_value::AsyncReturn();
    } else {
        return resolve_return_value::NoReturn();
    }
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
    ref->process->ClearAborterFunc();
    ref->scheduler->when_not_running(*(ref->current_task), [ref, value] {
        auto procthread = ref->current_task->get_resource<ProcThread>();
        auto signal = procthread != nullptr ? procthread->GetAndClearSigpending() : -1;
        if (signal > 0) {
            auto optSigaction = procthread->GetSigaction(signal);
            if (optSigaction) {
                ref->current_task->get_cpu_state().rax = value;
                callctx_async::HandleSignalInWhenNotRunning(ref->scheduler, ref->current_task, procthread, *optSigaction, signal);
            } else {
                ref->scheduler->evict_task_with_lock(*(ref->current_task));
            }
            return;
        }
        ref->current_task->get_cpu_state().rax = value;
        ref->current_task->set_blocked(false);
    });
}

void callctx_impl::KillAsync(std::shared_ptr<callctx_impl> ref) {
    ref->process->ClearAborterFunc();
    ref->scheduler->when_not_running(*(ref->current_task), [ref] {
        ref->current_task->set_end(true);
    });
}

void callctx_impl::EntrypointAsync(std::shared_ptr<callctx_impl> ref, uintptr_t entrypoint, uintptr_t fsBase,
                                   uintptr_t stackPtr) {
    ref->process->ClearAborterFunc();
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

class callctx_async_filter : public callctx_async {
private:
    std::shared_ptr<callctx_async> parent;
    std::function<resolve_return_value(resolve_return_value)> filter;
public:
    callctx_async_filter(const std::shared_ptr<callctx_async> &parent, const std::function<resolve_return_value(resolve_return_value)> &filter) : parent(parent), filter(filter) {}
    void async() override;
    void returnAsync(intptr_t value) override;
};

void callctx_async_filter::async() {
    parent->async();
}

void callctx_async_filter::returnAsync(intptr_t value) {
    auto result = filter(resolve_return_value::Return(value));
    if (result.hasValue) {
        parent->returnAsync(result.result);
    }
}

std::shared_ptr<callctx_impl> callctx_impl::ReturnFilter(std::shared_ptr<callctx_impl> impl,
                                                         std::function<resolve_return_value(resolve_return_value)> filter) {
    auto nested_impl = std::make_shared<callctx_impl>(*impl);
    nested_impl->async = std::make_shared<callctx_async_filter>(impl->async, filter);
    return nested_impl;
}

callctx::callctx(std::shared_ptr<callctx_async> async) : impl(new callctx_impl(async)) {
}

ProcThread &callctx::GetProcess() const {
    return impl->GetProcess();
}

void callctx::Aborter(const std::function<void()> &func) {
    callctx_impl::Aborter(impl, func);
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

resolve_return_value callctx::NestedWrite(intptr_t ptr, intptr_t len, std::function<resolve_return_value(void *)> func, std::function<resolve_return_value()> fault) const {
    return callctx_impl::NestedWrite(impl, ptr, len, func, fault);
}

resolve_return_value callctx::NestedReadString(intptr_t ptr, std::function<resolve_return_value(const std::string &)> func) const {
    return callctx_impl::NestedReadString(impl, ptr, func);
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

resolve_return_value callctx::ReturnFilter(std::function<resolve_return_value(resolve_return_value)> filter, std::function<resolve_return_value (std::shared_ptr<callctx>)> cll) {
    auto sh = std::make_shared<callctx>();
    sh->impl = callctx_impl::ReturnFilter(impl, filter);
    auto result = cll(sh);
    if (result.hasValue) {
        result = filter(result);
    }
    return result;
}
