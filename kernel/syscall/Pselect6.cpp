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

struct fdset {
    int32_t data[];

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
    void ForEach(int, const std::function<void (int)> &);
};

void fdset::ForEach(int n, const std::function<void(int)> &f) {
    std::function<void(int)> func{f};
    auto maxPos = n / (8 * sizeof(data[0]));
    if ((n % (8 * sizeof(data[0]))) != 0) {
        ++maxPos;
    }
    for (int i = 0; i < maxPos; i++) {
        std::cout << "fdset " << std::dec << i << ": 0x" << std::hex << data[i] << std::dec << "\n";
        if (data[i] != 0) {
            for (auto bit = StartOfPos(i); bit < n && bit < StartOfPos(i + 1); bit++) {
                if (IsSet(bit % (8 * sizeof(data[0])))) {
                    func(bit);
                }
            }
        }
    }
}

class SelectImpl {
private:
    SyscallCtx ctx;
public:
    explicit SelectImpl(const SyscallCtx &ctx);
    static void Select(std::shared_ptr<SelectImpl> ref, int n, fdset *inp, fdset *outp, fdset *exc);
    static bool KeepSubscription(std::shared_ptr<SelectImpl> ref, int fd);
    static void NotifyRead(std::shared_ptr<SelectImpl> ref, int fd);
};

SelectImpl::SelectImpl(const SyscallCtx &ctx) : ctx(ctx) {}

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
    std::cout << "DoSelect(" << std::dec << n << std::hex << ", 0x" << (uintptr_t) inp << ", 0x" << (uintptr_t) outp
              << ", 0x" << (uintptr_t) exc << std::dec << ")\n";
    {
        std::shared_ptr<fdset> subscribeTo{};
        {
            auto size = SizeOfFdSet(n);
            subscribeTo = std::shared_ptr<fdset>((fdset *) malloc(size));
            bzero(&(*subscribeTo), size);
        }
        if (inp != nullptr) {
            inp->ForEach(n, [&subscribeTo](int fd) {
                std::cout << "Select read " << std::dec << fd << "\n";
                subscribeTo->Set(fd);
            });
        }
        if (outp != nullptr) {
            outp->ForEach(n, [&subscribeTo](int fd) {
                std::cout << "Select write " << std::dec << fd << "\n";
                subscribeTo->Set(fd);
            });
        }
        if (exc != nullptr) {
            exc->ForEach(n, [&subscribeTo](int fd) {
                std::cout << "Select exc " << std::dec << fd << "\n";
                subscribeTo->Set(fd);
            });
        }
        subscribeTo->ForEach(n, [ref] (int fd) {
            auto fdesc = ref->ctx.GetProcess().get_file_descriptor(fd);
            if (fdesc.Valid()) {
                std::cout << "Subscribe " << std::dec << fd << "\n";
                class Select sel{ref};
                fdesc.GetHandler()->Subscribe(fd, sel);
            }
        });
    }
}

bool SelectImpl::KeepSubscription(std::shared_ptr<SelectImpl> ref, int fd) {
    return false;
}

void SelectImpl::NotifyRead(std::shared_ptr<SelectImpl> ref, int fd) {
    std::cout << "Notified read " << std::dec << fd << "\n";
}

Select::Select(const SyscallCtx &ctx, int n, fdset *inp, fdset *outp, fdset *exc) : impl(new SelectImpl(ctx)) {
    SelectImpl::Select(impl, n, inp, outp, exc);
}

bool Select::KeepSubscription(int fd) {
    return SelectImpl::KeepSubscription(impl, fd);
}

void Select::NotifyRead(int fd) {
    SelectImpl::NotifyRead(impl, fd);
}

resolve_return_value
Pselect6::DoSelect(SyscallCtx ctx, int n, fdset *inp, fdset *outp, fdset *exc, const timespec *timeout,
                   const sigset_t *sigset) {
    Select select{ctx, n, inp, outp, exc};
    return ctx.Async();
}

int64_t Pselect6::Call(int64_t n, int64_t uptr_inp, int64_t uptr_outp, int64_t uptr_excp, SyscallAdditionalParams &params) {
    int64_t uptr_timespec = params.Param5();
    int64_t uptr_sig = params.Param6();
    SyscallCtx ctx{params};
    if (n < 0) {
        return -EINVAL;
    }
    std::cout << "select(" << std::dec << n << ", 0x" << std::hex << uptr_inp << ", 0x" << uptr_outp << ", 0x"
              << uptr_excp << ", 0x" << uptr_timespec << ", 0x" << uptr_sig << ")\n";
    auto sizeofBitm = SizeOfFdSet(n);
    if (uptr_timespec != 0) {
        return ctx.Read(uptr_timespec, sizeof(timespec), [this, ctx, sizeofBitm, n, uptr_inp, uptr_outp, uptr_excp] (const void *ptr_timespec) {
            auto *timeout = (const timespec *) ptr_timespec;
            if (n == 0) {
                return DoSelect(ctx, 0, nullptr, nullptr, nullptr, timeout, nullptr);
            }
            if (uptr_inp != 0) {
                return ctx.NestedWrite(uptr_inp, sizeofBitm, [this, ctx, sizeofBitm, n, uptr_outp, uptr_excp, timeout] (void *ptr_inp) {
                    auto *inp = (fdset *) ptr_inp;
                    if (uptr_outp != 0) {
                        return ctx.NestedWrite(uptr_outp, sizeofBitm, [this, ctx, sizeofBitm, n, uptr_excp, inp, timeout] (void *ptr_outp) {
                            auto *outp = (fdset *) ptr_outp;
                            if (uptr_excp != 0) {
                                return ctx.NestedWrite(uptr_excp, sizeofBitm, [this, ctx, n, inp, outp, timeout] (void *ptr_excp) {
                                    auto *excp = (fdset *) ptr_excp;
                                    return DoSelect(ctx, n, inp, outp, excp, timeout, nullptr);
                                });
                            } else {
                                return DoSelect(ctx, n, inp, outp, nullptr, timeout, nullptr);
                            }
                        });
                    } else {
                        if (uptr_excp != 0) {
                            return ctx.NestedWrite(uptr_excp, sizeofBitm, [this, ctx, n, inp, timeout] (void *ptr_excp) {
                                auto *excp = (fdset *) ptr_excp;
                                return DoSelect(ctx, n, inp, nullptr, excp, timeout, nullptr);
                            });
                        } else {
                            return DoSelect(ctx, n, inp, nullptr, nullptr, timeout, nullptr);
                        }
                    }
                });
            } else {
                if (uptr_outp != 0) {
                    return ctx.NestedWrite(uptr_outp, sizeofBitm, [this, ctx, sizeofBitm, n, uptr_excp, timeout] (void *ptr_outp) {
                        auto *outp = (fdset *) ptr_outp;
                        if (uptr_excp != 0) {
                            return ctx.NestedWrite(uptr_excp, sizeofBitm, [this, ctx, n, outp, timeout] (void *ptr_excp) {
                                auto *excp = (fdset *) ptr_excp;
                                return DoSelect(ctx, n, nullptr, outp, excp, timeout, nullptr);
                            });
                        } else {
                            return DoSelect(ctx, n, nullptr, outp, nullptr, timeout, nullptr);
                        }
                    });
                } else {
                    if (uptr_excp != 0) {
                        return ctx.NestedWrite(uptr_excp, sizeofBitm, [this, ctx, n, timeout] (void *ptr_excp) {
                            auto *excp = (fdset *) ptr_excp;
                            return DoSelect(ctx, n, nullptr, nullptr, excp, timeout, nullptr);
                        });
                    } else {
                        return DoSelect(ctx, 0, nullptr, nullptr, nullptr, timeout, nullptr);
                    }
                }
            }
        });
    } else {
        if (n == 0) {
            return ctx.Await(DoSelect(ctx, 0, nullptr, nullptr, nullptr, nullptr, nullptr));
        }
        if (uptr_inp != 0) {
            return ctx.Write(uptr_inp, sizeofBitm, [this, ctx, sizeofBitm, n, uptr_outp, uptr_excp] (void *ptr_inp) {
                auto *inp = (fdset *) ptr_inp;
                if (uptr_outp != 0) {
                    return ctx.NestedWrite(uptr_outp, sizeofBitm, [this, ctx, sizeofBitm, n, uptr_excp, inp] (void *ptr_outp) {
                        auto *outp = (fdset *) ptr_outp;
                        if (uptr_excp != 0) {
                            return ctx.NestedWrite(uptr_excp, sizeofBitm, [this, ctx, n, inp, outp] (void *ptr_excp) {
                                auto *excp = (fdset *) ptr_excp;
                                return DoSelect(ctx, n, inp, outp, excp, nullptr, nullptr);
                            });
                        } else {
                            return DoSelect(ctx, n, inp, outp, nullptr, nullptr, nullptr);
                        }
                    });
                } else {
                    if (uptr_excp != 0) {
                        return ctx.NestedWrite(uptr_excp, sizeofBitm, [this, ctx, n, inp] (void *ptr_excp) {
                            auto *excp = (fdset *) ptr_excp;
                            return DoSelect(ctx, n, inp, nullptr, excp, nullptr, nullptr);
                        });
                    } else {
                        return DoSelect(ctx, n, inp, nullptr, nullptr, nullptr, nullptr);
                    }
                }
            });
        } else {
            if (uptr_outp != 0) {
                return ctx.Write(uptr_outp, sizeofBitm, [this, ctx, sizeofBitm, n, uptr_excp] (void *ptr_outp) {
                    auto *outp = (fdset *) ptr_outp;
                    if (uptr_excp != 0) {
                        return ctx.NestedWrite(uptr_excp, sizeofBitm, [this, ctx, n, outp] (void *ptr_excp) {
                            auto *excp = (fdset *) ptr_excp;
                            return DoSelect(ctx, n, nullptr, outp, excp, nullptr, nullptr);
                        });
                    } else {
                        return DoSelect(ctx, n, nullptr, outp, nullptr, nullptr, nullptr);
                    }
                });
            } else {
                if (uptr_excp != 0) {
                    return ctx.Write(uptr_excp, sizeofBitm, [this, ctx, n] (void *ptr_excp) {
                        auto *excp = (fdset *) ptr_excp;
                        return DoSelect(ctx, n, nullptr, nullptr, excp, nullptr, nullptr);
                    });
                } else {
                    return ctx.Await(DoSelect(ctx, 0, nullptr, nullptr, nullptr, nullptr, nullptr));
                }
            }
        }
    }
}