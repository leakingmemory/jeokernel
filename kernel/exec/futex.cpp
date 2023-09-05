//
// Created by sigsegv on 8/17/23.
//

#include <exec/futex.h>
#include <exec/procthread.h>
#include <exec/callctx.h>
#include <errno.h>

//#define FUTEX_DEBUG

#ifdef FUTEX_DEBUG
#include <iostream>
#endif

static Futex futex{};

Futex &Futex::Instance() {
    return futex;
}

int Futex::WakeAll(Process &process, uintptr_t uptr_addr) {
#ifdef FUTEX_DEBUG
    std::cout << "futex(" << std::hex << uptr_addr << std::dec << "): wake all\n";
#endif
    pid_t pid = process.getpid();
    typeof(waiting) wakeups{};
    {
        std::lock_guard lock{mtx};
        auto iterator = waiting.begin();
        while (iterator != waiting.end()) {
            const typeof(*iterator) &w = *iterator;
            if (w.pid == pid && w.addr == uptr_addr) {
                wakeups.push_back(w);
                waiting.erase(iterator);
            } else {
                ++iterator;
            }
        }
    }
    for (auto &w : wakeups) {
        w.wakeup();
    }
    return 0;
}

int Futex::Wake(Process &process, uintptr_t uptr_addr, int wakeNum) {
#ifdef FUTEX_DEBUG
    std::cout << "futex(" << std::hex << uptr_addr << "): wake " << std::dec << wakeNum << "\n";
#endif
    pid_t pid = process.getpid();
    typeof(waiting) wakeups{};
    {
        std::lock_guard lock{mtx};
        typeof(wakeNum) i = 0;
        auto iterator = waiting.begin();
        while (iterator != waiting.end() && i < wakeNum) {
            const typeof(*iterator) &w = *iterator;
            if (w.pid == pid && w.addr == uptr_addr) {
                wakeups.push_back(w);
                waiting.erase(iterator);
                i++;
            } else {
                ++iterator;
            }
        }
    }
    for (auto &w : wakeups) {
        w.wakeup();
    }
    return 0;
}

int Futex::Wait(std::shared_ptr<callctx> ctx, uintptr_t uptr_addr, uint32_t val) {
    auto tid = ctx->GetProcess().gettid();
    return ctx->Write(uptr_addr, sizeof(uint32_t), [this, ctx, val, uptr_addr, tid] (void *ptr) {
        uintptr_t addr = (uintptr_t) ptr;
        uint32_t current = val;
        asm("mov %0, %%rcx; mov %1, %%rax; lock cmpxchgl %%eax, (%%rcx); mov %%rax, %1"::"m"(addr), "m"(current) : "%rax", "%rcx");
        if (val == current) {
#ifdef FUTEX_DEBUG
            std::cout << "Futex " << std::hex << uptr_addr << std::dec << " considers going to sleep on tid=" << tid
                      << " value=" << current << "\n";
#endif
            auto asyncCtx = ctx->AsyncCtx();
            std::unique_lock lock{mtx};
            if (*((uint32_t *) ptr) == current) {
#ifdef FUTEX_DEBUG
                std::cout << "Futex " << std::hex << uptr_addr << std::dec << " confirms going to sleep on tid="
                          << tid << "\n";
#endif
                auto pid = ctx->GetProcess().getpid();
                auto tid = ctx->GetProcess().gettid();
                waiting.emplace_back([asyncCtx, uptr_addr, tid]() {
#ifdef FUTEX_DEBUG
                    std::cout << "Futex " << std::hex << uptr_addr << std::dec << " awakened on tid=" << tid
                              << "\n";
#endif
                    asyncCtx->returnAsync(0);
                }, uptr_addr, ctx->GetProcess().getpid(), ctx->GetProcess().gettid());
                ctx->GetProcess().AborterFunc([this, asyncCtx, uptr_addr, pid, tid] () {
                    bool found{false};
                    {
                        std::lock_guard lock{mtx};
                        auto iterator = waiting.begin();
                        while (iterator != waiting.end()) {
                            if (iterator->addr == uptr_addr && iterator->pid == pid && iterator->tid == tid) {
                                waiting.erase(iterator);
                                found = true;
                                break;
                            }
                            ++iterator;
                        }
                    }
                    if (found) {
#ifdef FUTEX_DEBUG
                        std::cout << "Futex " << std::hex << uptr_addr << std::dec << " awakened on tid=" << tid
                                  << "\n";
#endif
                        asyncCtx->returnAsync(-EINTR);
                    }
                });
                lock.release();
                if (ctx->GetProcess().HasPendingSignalOrKill()) {
                    ctx->GetProcess().CallAbort();
                }
                return resolve_return_value::AsyncReturn();
            } else {
                lock.release();
#ifdef FUTEX_DEBUG
                std::cout << "Futex " << std::hex << uptr_addr << std::dec << " reconsiders and returns on tid="
                          << tid << "\n";
#endif
                return resolve_return_value::Return(-EAGAIN);
            }
        } else {
#ifdef FUTEX_DEBUG
            std::cout << "Futex returns due " << std::dec << val << "!=" << current << " on tid=" << tid << "\n";
#endif
            return resolve_return_value::Return(-EAGAIN);
        }
    });
}