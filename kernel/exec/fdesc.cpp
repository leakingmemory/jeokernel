//
// Created by sigsegv on 6/18/22.
//

#include <exec/procthread.h>
#include <exec/fdesc.h>
#include <exec/usermem.h>
#include <errno.h>
#include <sys/uio.h>
#include <iostream>

//#define WRITEV_DEBUG

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

constexpr uintptr_t offset(uintptr_t ptr) {
    return ptr & (PAGESIZE-1);
}

struct kiovec : iovec {
    kiovec(iovec *usr) : vm(offset((uintptr_t) usr->iov_base) + usr->iov_len) {
        iov_base = (void *) ((uintptr_t) vm.pointer() + offset((uintptr_t) usr->iov_base));
        iov_len = usr->iov_len;
    }
    vmem vm;
};

struct writev_state {
    hw_spinlock lock{};
    std::vector<std::shared_ptr<kiovec>> iovecs{};
    bool failed{false};
    int remaining;
};

void
FileDescriptor::writev(ProcThread *process, uintptr_t usersp_iov_ptr, int iovcnt, std::function<void(intptr_t)> func) {
    auto handler = this->handler;
    std::shared_ptr<writev_state> state{new writev_state};
    state->remaining = iovcnt;
#ifdef WRITEV_DEBUG
    std::cout << "writev: remaining=" << state->remaining << "\n";
#endif
    process->resolve_read(usersp_iov_ptr, sizeof(iovec), [process, handler, usersp_iov_ptr, iovcnt, state, func] (bool success) mutable {
        if (success) {
            auto iov_len = sizeof(iovec) * iovcnt;
            UserMemory umem{*process, usersp_iov_ptr, iov_len};
            iovec *iov = (iovec *) umem.Pointer();
            for (int i = 0; i < iovcnt; i++) {
#ifdef WRITEV_DEBUG
                std::cout << "writev: iov: 0x" << std::hex << (uintptr_t) iov[i].iov_base << std::dec
                          << " len="<< iov[i].iov_len << "\n";
#endif
                state->iovecs.emplace_back(new kiovec(&(iov[i])));
                if (iov[i].iov_len == 0) {
                    state->remaining--;
                }
            }
            if (state->remaining == 0) {
                func(0);
                return;
            }
#ifdef WRITEV_DEBUG
            std::cout << "writev: resolve buffers, remaining=" << state->remaining << "\n";
#endif
            for (int i = 0; i < iovcnt; i++) {
                if (iov[i].iov_len == 0) {
                    continue;
                }
                process->resolve_read((uintptr_t) iov[i].iov_base, iov[i].iov_len, [handler, process, umem, iov, iovcnt, i, state, func] (bool success) mutable {
                    std::unique_lock lock{state->lock};
                    if (state->failed) {
                        return;
                    }
                    if (!success) {
#ifdef WRITEV_DEBUG
                        std::cerr << "writev: invalid buffer from usersp << " << std::hex << (uintptr_t) iov[i].iov_base << std::dec << "\n";
#endif
                        state->failed = true;
                        lock.release();
                        func(-EFAULT);
                        return;
                    }
                    state->remaining--;
                    auto &vm = state->iovecs[i]->vm;
                    auto &iovo = iov[i];
                    auto vaddr = (uintptr_t) iovo.iov_base;
                    auto offset = vaddr & (PAGESIZE-1);
                    vaddr -= offset;
                    for (int p = 0; p < vm.npages(); p++) {
                        auto paddr = process->phys_addr(vaddr);
                        if (paddr == 0) {
#ifdef WRITEV_DEBUG
                            std::cerr << "writev: invalid buffer from usersp << " << std::hex << (uintptr_t) iov[i].iov_base << std::dec << "\n";
#endif
                            state->failed = true;
                            lock.release();
                            func(-EFAULT);
                            return;
                        }
                        vm.page(p).rmap(paddr);
                        vaddr += PAGESIZE;
                    }
                    state->iovecs[i]->iov_base = (void *) ((uintptr_t) vm.pointer() + offset);
                    state->iovecs[i]->iov_len = iovo.iov_len;
#ifdef WRITEV_DEBUG
                    std::cout << "writev: buffer=0x"<< std::hex << (uintptr_t) state->iovecs[i]->iov_base
                              << " len=" << std::dec << state->iovecs[i]->iov_len << "\n";
#endif
                    if (state->remaining == 0) {
#ifdef WRITEV_DEBUG
                        std::cout << "writev: final\n";
#endif
                        lock.release();
                        vm.reload();
                        intptr_t totlen{0};
                        for (int i = 0; i < iovcnt; i++) {
                            handler->write(state->iovecs[i]->iov_base, (intptr_t) state->iovecs[i]->iov_len);
                            totlen += state->iovecs[i]->iov_len;
                        }
                        func(totlen);
                    }
                });
            }
        } else {
            func(-EFAULT);
        }
    });
}
