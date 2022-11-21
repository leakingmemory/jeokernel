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
//#define SUBSCRIPTION_DEBUG

FileDescriptorHandler::FileDescriptorHandler() : mtx(), subscriptions(), readyRead(false) {
}

FileDescriptorHandler::FileDescriptorHandler(const FileDescriptorHandler &cp) : mtx(), subscriptions(), readyRead(cp.readyRead) {
}

void FileDescriptorHandler::Subscribe(int fd, Select select) {
    std::lock_guard lock{mtx};
    if (readyRead) {
        select.NotifyRead(fd);
    }
    if (select.KeepSubscription(fd)) {
        subscriptions.push_back({.impl = select, .fd = fd});
    }
}

void FileDescriptorHandler::SetReadyRead(bool ready) {
    std::vector<FdSubscription> notify{};
    {
        std::lock_guard lock{mtx};
        if (this->readyRead != ready) {
            this->readyRead = ready;
            if (this->readyRead) {
                auto iterator = this->subscriptions.begin();
                while (iterator != this->subscriptions.end()) {
                    notify.push_back(*iterator);
                    ++iterator;
                }
            }
        }
    }
    std::vector<FdSubscription> remove{};
    for (auto &subscription : notify) {
        subscription.impl.NotifyRead(subscription.fd);
        if (!subscription.impl.KeepSubscription(subscription.fd)) {
            remove.push_back(subscription);
        }
    }
    std::lock_guard lock{mtx};
    auto iterator = this->subscriptions.begin();
    while (iterator != this->subscriptions.end()) {
        bool found{false};
        for (auto &rem : remove) {
            if (rem.impl == iterator->impl && rem.fd == iterator->fd) {
                found = true;
                break;
            }
        }
        if (found) {
#ifdef SUBSCRIPTION_DEBUG
            std::cout << "Drop subscription " << std::dec << iterator->fd << "\n";
#endif
            this->subscriptions.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void FileDescriptorHandler::Notify() {
}

std::shared_ptr<kfile> FileDescriptor::get_file() const {
    return handler->get_file();
}

bool FileDescriptor::can_read() {
    return handler->can_read();
}

resolve_return_value FileDescriptor::read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len) {
    return handler->read(ctx, ptr, len);
}

resolve_return_value FileDescriptor::read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len, uintptr_t offset) {
    return handler->read(ctx, ptr, len, offset);
}

file_descriptor_result FileDescriptor::write(ProcThread *process, uintptr_t usersp_ptr, intptr_t len, std::function<void (intptr_t)> func) {
    auto handler = this->handler;
    auto result = process->resolve_read(usersp_ptr, len, false, func, [process, handler, usersp_ptr, len, func] (bool success, bool, std::function<void (intptr_t)>) mutable {
        if (success) {
            UserMemory umem{*process, usersp_ptr, (uintptr_t) len};
            if (!umem) {
                return resolve_return_value::Return(-EFAULT);
            }
            return resolve_return_value::Return(handler->write((const void *) umem.Pointer(), len));
        } else {
            return resolve_return_value::Return(-EFAULT);
        }
    });
    return {.result = result.result, .async = result.async};
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

file_descriptor_result
FileDescriptor::writev(ProcThread *process, uintptr_t usersp_iov_ptr, int iovcnt, std::function<void(intptr_t)> func) {
    auto handler = this->handler;
    std::shared_ptr<writev_state> state{new writev_state};
    state->remaining = iovcnt;
#ifdef WRITEV_DEBUG
    std::cout << "writev: remaining=" << state->remaining << "\n";
#endif
    auto result = process->resolve_read(usersp_iov_ptr, sizeof(iovec), false, [func] (uintptr_t returnv) mutable {func(returnv);}, [process, handler, usersp_iov_ptr, iovcnt, state, func] (bool success, bool async, std::function<void (intptr_t)> asyncFunc) mutable {
        if (success) {
            auto iov_len = sizeof(iovec) * iovcnt;
            UserMemory umem{*process, usersp_iov_ptr, iov_len};
            if (!umem) {
                return resolve_return_value::Return(-EFAULT);
            }
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
                return resolve_return_value::Return(0);
            }
#ifdef WRITEV_DEBUG
            std::cout << "writev: resolve buffers, remaining=" << state->remaining << "\n";
#endif
            auto nestedAsync = async;
            for (int i = 0; i < iovcnt; i++) {
                if (iov[i].iov_len == 0) {
                    continue;
                }
                auto nestedResult = process->resolve_read((uintptr_t) iov[i].iov_base, iov[i].iov_len, async, asyncFunc, [handler, process, umem, iov, iovcnt, i, state, func] (bool success, bool, const std::function<void (uintptr_t)> &) mutable {
                    std::unique_lock lock{state->lock};
                    if (state->failed) {
                        return resolve_return_value::NoReturn();
                    }
                    if (!success) {
#ifdef WRITEV_DEBUG
                        std::cerr << "writev: invalid buffer from usersp << " << std::hex << (uintptr_t) iov[i].iov_base << std::dec << "\n";
#endif
                        state->failed = true;
                        lock.release();
                        return resolve_return_value::Return(-EFAULT);
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
                            return resolve_return_value::Return(-EFAULT);
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
                        return resolve_return_value::Return(totlen);
                    }
                    return resolve_return_value::NoReturn();
                });
                if (nestedResult.hasValue) {
                    if (nestedAsync) {
                        asyncFunc(nestedResult.result);
                        return resolve_return_value::AsyncReturn();
                    }
                    return resolve_return_value::Return(nestedResult.result);
                }
                nestedAsync |= nestedResult.async;
            }
            if (nestedAsync) {
                return resolve_return_value::AsyncReturn();
            }
            std::cerr << "Not reach: Fell through (fdesc write)\n";
            return resolve_return_value::Return(-ENOMEM);
        } else {
            return resolve_return_value::Return(-EFAULT);
        }
    });
    return {.result = result.result, .async = result.async};
}

bool FileDescriptor::stat(struct stat &st) {
    return handler->stat(st);
}

intptr_t FileDescriptor::ioctl(callctx &ctx, intptr_t cmd, intptr_t arg) {
    return handler->ioctl(ctx, cmd, arg);
}

int FileDescriptor::readdir(const std::function<bool (kdirent &dirent)> &func) const {
    return handler->readdir(func);
}