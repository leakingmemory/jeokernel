//
// Created by sigsegv on 10/11/22.
//

#include "Pselect6.h"
#include <errno.h>
#include <time.h>
#include <iostream>
#include <exec/fdselect.h>
#include <strings.h>
#include <exec/procthread.h>

//#define SELECT_DEBUG
//#define FDSET_DEBUG

struct fdset {
    int32_t data[0];

private:
    constexpr int StartOfPos(int pos) {
        return pos * sizeof(data[0]) * 8;
    }
public:
    bool IsSet(int n) {
        auto pos = n / (8 * sizeof(data[0]));
        auto bit = n % (8 * sizeof(data[0]));
        auto mask = ((typeof(data[0])) 1) << bit;
        return ((data[pos] & mask) != 0);
    }
    void Set(int n) {
        auto pos = n / (8 * sizeof(data[0]));
        auto bit = n % (8 * sizeof(data[0]));
        auto mask = ((typeof(data[0])) 1) << bit;
        data[pos] |= mask;
    }
    void Unset(int n) {
        auto pos = n / (8 * sizeof(data[0]));
        auto bit = n % (8 * sizeof(data[0]));
        auto mask = ((typeof(data[0])) 1) << bit;
        data[pos] &= ~mask;
    }
    void ForEach(int, const std::function<void (int)> &);
    bool AnySet(int);
};

void fdset::ForEach(int n, const std::function<void(int)> &f) {
    std::function<void(int)> func{f};
    auto maxPos = n / (8 * sizeof(data[0]));
    if ((n % (8 * sizeof(data[0]))) != 0) {
        ++maxPos;
    }
    for (int i = 0; i < maxPos; i++) {
#ifdef FDSET_DEBUG
        std::cout << "fdset " << std::dec << i << ": 0x" << std::hex << data[i] << std::dec << "\n";
#endif
        if (data[i] != 0) {
            for (auto bit = StartOfPos(i); bit < n && bit < StartOfPos(i + 1); bit++) {
                if (IsSet(bit % (8 * sizeof(data[0])))) {
                    func(bit);
                }
            }
        }
    }
}

bool fdset::AnySet(int n) {
    auto maxPos = n / (8 * sizeof(data[0]));
    if ((n % (8 * sizeof(data[0]))) != 0) {
        ++maxPos;
    }
    for (int i = 0; i < maxPos; i++) {
        if (data[i] != 0) {
            if (i < (maxPos - 1)) {
                return true;
            }
            for (auto bit = StartOfPos(i); bit < n && bit < StartOfPos(i + 1); bit++) {
                if (IsSet(bit % (8 * sizeof(data[0])))) {
                    return true;
                }
            }
        }
    }
    return false;
}

class SelectImpl {
private:
    SyscallCtx ctx;
    hw_spinlock mtx;
    int n, sig_n;
    fdset *u_inp, *u_outp, *u_exc;
    fdset *inp_sel;
    fdset *inp_sig;
    std::function<void (const std::function<void ()> &)> submitAsync;
    bool wake;
public:
    explicit SelectImpl(const SyscallCtx &ctx, const std::function<void (const std::function<void ()> &)> &submitAsync);
    ~SelectImpl();
    SelectImpl(const SelectImpl &) = delete;
    SelectImpl(SelectImpl &&) = delete;
    SelectImpl &operator =(const SelectImpl &) = delete;
    SelectImpl &operator =(SelectImpl &&) = delete;
private:
    void Clear();
public:
    static void Select(std::shared_ptr<SelectImpl> ref, int n, fdset *inp, fdset *outp, fdset *exc);
    static bool KeepSubscription(std::shared_ptr<SelectImpl> ref, int fd);
    static void CopyBack(std::shared_ptr<SelectImpl> ref);
    static void NotifyRead(std::shared_ptr<SelectImpl> ref, int fd);
    static void NotifyIntr(std::shared_ptr<SelectImpl> ref);
};

SelectImpl::SelectImpl(const SyscallCtx &ctx, const std::function<void (const std::function<void ()> &)> &submitAsync) : ctx(ctx), mtx(), n(0), sig_n(0), u_inp(nullptr), u_outp(nullptr), u_exc(nullptr), inp_sel(nullptr), inp_sig(nullptr), submitAsync(submitAsync), wake(true) {}

SelectImpl::~SelectImpl() {
    Clear();
}

void SelectImpl::Clear() {
    if (inp_sel != nullptr) {
        free(inp_sel);
        inp_sel = nullptr;
    }
    if (inp_sig != nullptr) {
        free(inp_sig);
        inp_sig = nullptr;
    }
}

constexpr intptr_t SizeOfFdSet(intptr_t n) {
    auto sizeofBitm = n >> 3;
    if ((n & 7) != 0) {
        ++sizeofBitm;
    }
    if ((sizeofBitm & 3) != 0) {
        sizeofBitm += 4 - (sizeofBitm & 3);
    }
    return sizeofBitm;
}

void SelectImpl::Select(std::shared_ptr<SelectImpl> ref, int n, fdset *inp, fdset *outp, fdset *exc) {
#ifdef SELECT_DEBUG
    std::cout << "DoSelect(" << std::dec << n << std::hex << ", 0x" << (uintptr_t) inp << ", 0x" << (uintptr_t) outp
              << ", 0x" << (uintptr_t) exc << std::dec << ")\n";
#endif
    std::unique_lock lock{ref->mtx};
    ref->Clear();
    ref->n = n;
    ref->sig_n = 0;
    ref->u_inp = inp;
    ref->u_outp = outp;
    ref->u_exc = exc;
    if (n > 0) {
        auto size = SizeOfFdSet(n);
        if (inp != nullptr) {
            ref->inp_sel = (fdset *) malloc(size);
            ref->inp_sig = (fdset *) malloc(size);
            bzero(ref->inp_sel, size);
            bzero(ref->inp_sig, size);
        }
    }
    {
        std::shared_ptr<fdset> subscribeTo{};
        {
            auto size = SizeOfFdSet(n);
            subscribeTo = std::shared_ptr<fdset>((fdset *) malloc(size));
            bzero(&(*subscribeTo), size);
        }
        if (inp != nullptr) {
            inp->ForEach(n, [&subscribeTo, ref](int fd) {
#ifdef SELECT_DEBUG
                std::cout << "Select read " << std::dec << fd << "\n";
#endif
                ref->inp_sel->Set(fd);
                subscribeTo->Set(fd);
            });
        }
        if (outp != nullptr) {
            outp->ForEach(n, [&subscribeTo](int fd) {
#ifdef SELECT_DEBUG
                std::cout << "Select write " << std::dec << fd << "\n";
#endif
                subscribeTo->Set(fd);
            });
        }
        if (exc != nullptr) {
            exc->ForEach(n, [&subscribeTo](int fd) {
#ifdef SELECT_DEBUG
                std::cout << "Select exc " << std::dec << fd << "\n";
#endif
                subscribeTo->Set(fd);
            });
        }
        lock.release();
        subscribeTo->ForEach(n, [ref] (int fd) {
            auto fdesc = ref->ctx.GetProcess().get_file_descriptor(fd);
            if (fdesc.Valid()) {
#ifdef SELECT_DEBUG
                std::cout << "Subscribe " << std::dec << fd << "\n";
#endif
                class Select sel{ref};
                fdesc.GetHandler()->Subscribe(fd, sel);
            }
        });
    }
}

bool SelectImpl::KeepSubscription(std::shared_ptr<SelectImpl> ref, int fd) {
    std::lock_guard lock{ref->mtx};
    if (ref->inp_sel != nullptr) {
        if (ref->inp_sel->AnySet(ref->n)) {
            return true;
        }
    }
    return false;
}

void SelectImpl::CopyBack(std::shared_ptr<SelectImpl> ref) {
    auto size = SizeOfFdSet(ref->n);
    if (ref->u_inp != nullptr) {
        memcpy(ref->u_inp, ref->inp_sig, size);
    }
    if (ref->u_outp != nullptr) {
        bzero(ref->u_outp, size);
    }
    if (ref->u_exc != nullptr) {
        bzero(ref->u_exc, size);
    }
}

void SelectImpl::NotifyRead(std::shared_ptr<SelectImpl> ref, int fd) {
    bool wake;
    {
#ifdef SELECT_DEBUG
        std::cout << "Notified read " << std::dec << fd << "\n";
#endif
        std::lock_guard lock{ref->mtx};
        wake = ref->wake;
        ref->wake = false;
        if (fd < ref->n) {
            ref->inp_sel->Unset(fd);
            ref->inp_sig->Set(fd);
            ++(ref->sig_n);
        }
    }
    if (wake) {
        ref->submitAsync([ref] () {
            int retv;
            {
                std::lock_guard lock{ref->mtx};
                SelectImpl::CopyBack(ref);
                retv = ref->sig_n;
            }
#ifdef SELECT_DEBUG
            std::cout << "Pselect6 return " << retv << "\n";
#endif
            ref->ctx.ReturnWhenNotRunning(retv);
        });
    }
}

void SelectImpl::NotifyIntr(std::shared_ptr<SelectImpl> ref) {
    bool wake;
    {
#ifdef SELECT_DEBUG
        std::cout << "Notified read " << std::dec << fd << "\n";
#endif
        std::lock_guard lock{ref->mtx};
        wake = ref->wake;
        ref->wake = false;
    }
    if (wake) {
        ref->submitAsync([ref] () {
            ref->ctx.ReturnWhenNotRunning(-EINTR);
        });
    }
}

Select::Select(const SyscallCtx &ctx, const std::function<void (const std::function<void ()> &)> &submitAsync, int n, fdset *inp, fdset *outp, fdset *exc) : impl(new SelectImpl(ctx, submitAsync)) {
    SelectImpl::Select(impl, n, inp, outp, exc);
}

bool Select::KeepSubscription(int fd) const {
    return SelectImpl::KeepSubscription(impl, fd);
}

void Select::NotifyRead(int fd) const {
    SelectImpl::NotifyRead(impl, fd);
}

void Select::NotifyIntr() const {
    SelectImpl::NotifyIntr(impl);
}

resolve_return_value
Pselect6::DoSelect(SyscallCtx ctx, uint32_t task_id, int n, fdset *inp, fdset *outp, fdset *exc, const timespec *timeout,
                   const sigset_t *sigset) {
    Select select{ctx, [this, task_id] (const std::function<void ()> &func) { Queue(task_id, func); }, n, inp, outp, exc};
    ctx.Aborter([select] () {
        select.NotifyIntr();
    });
    return ctx.Async();
}

int64_t Pselect6::Call(int64_t n, int64_t uptr_inp, int64_t uptr_outp, int64_t uptr_excp, SyscallAdditionalParams &params) {
    int64_t uptr_timespec = params.Param5();
    int64_t uptr_sig = params.Param6();
    SyscallCtx ctx{params};
    if (n < 0) {
        return -EINVAL;
    }
#ifdef SELECT_DEBUG
    std::cout << "select(" << std::dec << n << ", 0x" << std::hex << uptr_inp << ", 0x" << uptr_outp << ", 0x"
              << uptr_excp << ", 0x" << uptr_timespec << ", 0x" << uptr_sig << ")\n";
#endif
    auto sizeofBitm = SizeOfFdSet(n);
    auto task_id = get_scheduler()->get_current_task_id();
    if (uptr_timespec != 0) {
        return ctx.Read(uptr_timespec, sizeof(timespec), [this, ctx, task_id, sizeofBitm, n, uptr_inp, uptr_outp, uptr_excp] (const void *ptr_timespec) {
            auto *timeout = (const timespec *) ptr_timespec;
            if (n == 0) {
                return DoSelect(ctx, task_id, 0, nullptr, nullptr, nullptr, timeout, nullptr);
            }
            if (uptr_inp != 0) {
                return ctx.NestedWrite(uptr_inp, sizeofBitm, [this, ctx, task_id, sizeofBitm, n, uptr_outp, uptr_excp, timeout] (void *ptr_inp) {
                    auto *inp = (fdset *) ptr_inp;
                    if (uptr_outp != 0) {
                        return ctx.NestedWrite(uptr_outp, sizeofBitm, [this, ctx, task_id, sizeofBitm, n, uptr_excp, inp, timeout] (void *ptr_outp) {
                            auto *outp = (fdset *) ptr_outp;
                            if (uptr_excp != 0) {
                                return ctx.NestedWrite(uptr_excp, sizeofBitm, [this, ctx, task_id, n, inp, outp, timeout] (void *ptr_excp) {
                                    auto *excp = (fdset *) ptr_excp;
                                    return DoSelect(ctx, task_id, n, inp, outp, excp, timeout, nullptr);
                                });
                            } else {
                                return DoSelect(ctx, task_id, n, inp, outp, nullptr, timeout, nullptr);
                            }
                        });
                    } else {
                        if (uptr_excp != 0) {
                            return ctx.NestedWrite(uptr_excp, sizeofBitm, [this, ctx, task_id, n, inp, timeout] (void *ptr_excp) {
                                auto *excp = (fdset *) ptr_excp;
                                return DoSelect(ctx, task_id, n, inp, nullptr, excp, timeout, nullptr);
                            });
                        } else {
                            return DoSelect(ctx, task_id, n, inp, nullptr, nullptr, timeout, nullptr);
                        }
                    }
                });
            } else {
                if (uptr_outp != 0) {
                    return ctx.NestedWrite(uptr_outp, sizeofBitm, [this, ctx, task_id, sizeofBitm, n, uptr_excp, timeout] (void *ptr_outp) {
                        auto *outp = (fdset *) ptr_outp;
                        if (uptr_excp != 0) {
                            return ctx.NestedWrite(uptr_excp, sizeofBitm, [this, ctx, task_id, n, outp, timeout] (void *ptr_excp) {
                                auto *excp = (fdset *) ptr_excp;
                                return DoSelect(ctx, task_id, n, nullptr, outp, excp, timeout, nullptr);
                            });
                        } else {
                            return DoSelect(ctx, task_id, n, nullptr, outp, nullptr, timeout, nullptr);
                        }
                    });
                } else {
                    if (uptr_excp != 0) {
                        return ctx.NestedWrite(uptr_excp, sizeofBitm, [this, ctx, task_id, n, timeout] (void *ptr_excp) {
                            auto *excp = (fdset *) ptr_excp;
                            return DoSelect(ctx, task_id, n, nullptr, nullptr, excp, timeout, nullptr);
                        });
                    } else {
                        return DoSelect(ctx, task_id, 0, nullptr, nullptr, nullptr, timeout, nullptr);
                    }
                }
            }
        });
    } else {
        if (n == 0) {
            return ctx.Await(DoSelect(ctx, task_id, 0, nullptr, nullptr, nullptr, nullptr, nullptr));
        }
        if (uptr_inp != 0) {
            return ctx.Write(uptr_inp, sizeofBitm, [this, ctx, task_id, sizeofBitm, n, uptr_outp, uptr_excp] (void *ptr_inp) {
                auto *inp = (fdset *) ptr_inp;
                if (uptr_outp != 0) {
                    return ctx.NestedWrite(uptr_outp, sizeofBitm, [this, ctx, task_id, sizeofBitm, n, uptr_excp, inp] (void *ptr_outp) {
                        auto *outp = (fdset *) ptr_outp;
                        if (uptr_excp != 0) {
                            return ctx.NestedWrite(uptr_excp, sizeofBitm, [this, ctx, task_id, n, inp, outp] (void *ptr_excp) {
                                auto *excp = (fdset *) ptr_excp;
                                return DoSelect(ctx, task_id, n, inp, outp, excp, nullptr, nullptr);
                            });
                        } else {
                            return DoSelect(ctx, task_id, n, inp, outp, nullptr, nullptr, nullptr);
                        }
                    });
                } else {
                    if (uptr_excp != 0) {
                        return ctx.NestedWrite(uptr_excp, sizeofBitm, [this, ctx, task_id, n, inp] (void *ptr_excp) {
                            auto *excp = (fdset *) ptr_excp;
                            return DoSelect(ctx, task_id, n, inp, nullptr, excp, nullptr, nullptr);
                        });
                    } else {
                        return DoSelect(ctx, task_id, n, inp, nullptr, nullptr, nullptr, nullptr);
                    }
                }
            });
        } else {
            if (uptr_outp != 0) {
                return ctx.Write(uptr_outp, sizeofBitm, [this, ctx, task_id, sizeofBitm, n, uptr_excp] (void *ptr_outp) {
                    auto *outp = (fdset *) ptr_outp;
                    if (uptr_excp != 0) {
                        return ctx.NestedWrite(uptr_excp, sizeofBitm, [this, ctx, task_id, n, outp] (void *ptr_excp) {
                            auto *excp = (fdset *) ptr_excp;
                            return DoSelect(ctx, task_id, n, nullptr, outp, excp, nullptr, nullptr);
                        });
                    } else {
                        return DoSelect(ctx, task_id, n, nullptr, outp, nullptr, nullptr, nullptr);
                    }
                });
            } else {
                if (uptr_excp != 0) {
                    return ctx.Write(uptr_excp, sizeofBitm, [this, ctx, task_id, n] (void *ptr_excp) {
                        auto *excp = (fdset *) ptr_excp;
                        return DoSelect(ctx, task_id, n, nullptr, nullptr, excp, nullptr, nullptr);
                    });
                } else {
                    return ctx.Await(DoSelect(ctx, task_id, 0, nullptr, nullptr, nullptr, nullptr, nullptr));
                }
            }
        }
    }
}