//
// Created by sigsegv on 6/18/22.
//

#include <exec/procthread.h>
#include <exec/fdesc.h>
#include <exec/usermem.h>
#include <errno.h>
#include <sys/uio.h>
#include <iostream>

//#define READV_DEBUG
//#define WRITEV_DEBUG
//#define WRITEV_IGNORE
//#define SUBSCRIPTION_DEBUG

FileDescriptorHandler::FileDescriptorHandler() : fdmtx(), subscriptions(), readyRead(false) {
}

FileDescriptorHandler::FileDescriptorHandler(const FileDescriptorHandler &cp) : fdmtx(), subscriptions(), readyRead(cp.readyRead) {
}

FileDescriptorHandler *FileDescriptorHandler::GetResource() {
    return this;
}

void FileDescriptorHandler::CopyFrom(const FileDescriptorHandler &cp) {
    readyRead = cp.readyRead;
}

void FileDescriptorHandler::Subscribe(int fd, Select select) {
    std::lock_guard lock{fdmtx};
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
        std::lock_guard lock{fdmtx};
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
    std::lock_guard lock{fdmtx};
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

reference<FileDescriptor>
FileDescriptor::CreateFromPointer(const std::shared_ptr<class referrer> &referrer, const std::shared_ptr<FileDescriptorHandler> &handler, int fd, int openFlags) {
    std::shared_ptr<FileDescriptor> fdesc{new FileDescriptor(fd, openFlags)};
    fdesc->SetSelfRef(fdesc);
    std::weak_ptr<FileDescriptor> weakPtr{fdesc};
    fdesc->selfRef = weakPtr;
    fdesc->handler = handler->CreateReference(fdesc);
    return fdesc->CreateReference(referrer);
}

reference<FileDescriptor>
FileDescriptor::Create(const std::shared_ptr<class referrer> &referrer, const reference<FileDescriptorHandler> &handler, int fd, int openFlags) {
    std::shared_ptr<FileDescriptor> fdesc{new FileDescriptor(fd, openFlags)};
    fdesc->SetSelfRef(fdesc);
    std::weak_ptr<FileDescriptor> weakPtr{fdesc};
    fdesc->selfRef = weakPtr;
    fdesc->handler = handler.CreateReference(fdesc);
    return fdesc->CreateReference(referrer);
}

std::string FileDescriptor::GetReferrerIdentifier() {
    return "";
}

FileDescriptor *FileDescriptor::GetResource() {
    return this;
}

void FileDescriptor::Subscribe(int fd, Select select) {
    handler->Subscribe(fd, select);
}

reference<kfile> FileDescriptor::get_file(std::shared_ptr<class referrer> &referrer) const {
    return handler->get_file(referrer);
}

bool FileDescriptor::can_seek() {
    return handler->can_seek();
}

bool FileDescriptor::can_read() {
    return handler->can_read();
}

intptr_t FileDescriptor::seek(intptr_t offset, SeekWhence whence) {
    return handler->seek(offset, whence);
}

resolve_return_value FileDescriptor::read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len) {
    return handler->read(ctx, ptr, len);
}

resolve_return_value FileDescriptor::read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len, uintptr_t offset) {
    return handler->read(ctx, ptr, len, offset);
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

intptr_t
FileDescriptor::readv(std::shared_ptr<callctx> ctx, uintptr_t usersp_iov_ptr, int iovcnt) {
    auto selfRef = this->selfRef.lock();
    std::shared_ptr<writev_state> state{new writev_state};
    state->remaining = iovcnt;
    auto task_id = get_scheduler()->get_current_task_id();
    return ctx->Read(usersp_iov_ptr, sizeof(iovec) * iovcnt, [ctx, state, selfRef, iovcnt, task_id] (void *ptr) mutable {
        iovec *iov = (iovec *) ptr;
        for (int i = 0; i < iovcnt; i++) {
            state->iovecs.emplace_back(new kiovec(&(iov[i])));
            if (iov[i].iov_len == 0) {
                state->remaining--;
            }
        }
        if (state->remaining == 0) {
            return resolve_return_value::Return(0);
        }
        for (int i = 0; i < iovcnt; i++) {
            if (iov[i].iov_len == 0) {
                continue;
            }
            auto nestedResult = ctx->NestedWrite((uintptr_t) iov[i].iov_base, iov[i].iov_len, [ctx, selfRef, iov, iovcnt, i, state, task_id] (void *bufptr) mutable {
                std::unique_lock lock{state->lock};
                if (state->failed) {
                    return resolve_return_value::NoReturn();
                }
                state->remaining--;
                auto &iovo = iov[i];
                state->iovecs[i]->iov_base = bufptr;
                state->iovecs[i]->iov_len = iovo.iov_len;
                if (state->remaining == 0) {
                    lock.release();
                    auto queue = std::make_shared<std::vector<std::shared_ptr<kiovec>>>();
                    for (int i = 0; i < iovcnt; i++) {
                        if (state->iovecs[i]->iov_len >= 0) {
                            queue->push_back(state->iovecs[i]);
                        }
                    }
                    auto processNext = std::make_shared<std::function<void ()>>();
                    auto totlen = std::make_shared<uintptr_t>(0);
                    *processNext = [state, ctx, selfRef, queue, processNext, totlen, task_id] () mutable {
                        std::shared_ptr<kiovec> iovec{};
                        {
                            auto iterator = queue->begin();
                            if (iterator != queue->end()) {
                                iovec = *iterator;
                                queue->erase(iterator);
                            }
                        }
                        if (iovec) {
                            ctx->ReturnFilter([state, ctx, totlen, iovec, queue] (auto result) {
                                std::unique_lock lock{state->lock};
#ifdef READV_DEBUG
                                std::cout << "iovec: result = " << result.result << "\n";
#endif
                                if (result.hasValue) {
                                    auto rval = result.result;
                                    if (rval >= 0) {
                                        *totlen += rval;
                                        if (rval < iovec->iov_len) {
                                            queue->clear();
                                        }
                                    } else {
                                        queue->clear();
                                        if (*totlen == 0) {
                                            state->failed = true;
                                            lock.release();
                                            ctx->AsyncCtx()->returnAsync(result.result);
                                            return resolve_return_value::NoReturn();
                                        }
                                    }
                                }
                                lock.release();
                                return resolve_return_value::NoReturn();
                            }, [selfRef, iovec] (auto ctx) {
                                return selfRef->handler->read(ctx, iovec->iov_base, iovec->iov_len);
                            });
                        }
                        if (queue->empty()) {
                            *processNext = [] () {};
                            std::unique_lock lock{state->lock};
                            if (!state->failed) {
                                lock.release();
#ifdef READV_DEBUG
                                std::cout << "iovec: all done = " << *totlen << "\n";
#endif
                                ctx->AsyncCtx()->returnAsync(*totlen);
                            }
                        } else {
                            ctx->GetProcess().QueueBlocking(task_id, *processNext);
                        }
                    };
                    if (!queue->empty()) {
#ifdef READV_DEBUG
                        std::cout << "readv: start processing " << queue->size() << " iovecs\n";
#endif
                        ctx->GetProcess().QueueBlocking(task_id, *processNext);
                        return resolve_return_value::AsyncReturn();
                    } else {
                        *processNext = [] () {};
                    }
                    return resolve_return_value::Return(0);
                }
                return resolve_return_value::NoReturn();
            }, /*fault:*/ [state] () {
                std::unique_lock lock{state->lock};
                state->failed = true;
                lock.release();
                return resolve_return_value::Return(-EFAULT);
            });
            if (nestedResult.hasValue) {
                return resolve_return_value::Return(nestedResult.result);
            }
        }
        return resolve_return_value::AsyncReturn();
    });
}

file_descriptor_result FileDescriptor::write(ProcThread *process, uintptr_t usersp_ptr, intptr_t len, std::function<void (intptr_t)> func) {
    auto selfRef = this->selfRef.lock();
    auto result = process->resolve_read(usersp_ptr, len, false, func, [process, selfRef, usersp_ptr, len, func] (bool success, bool, std::function<void (intptr_t)>) mutable {
        if (success) {
            UserMemory umem{*process, usersp_ptr, (uintptr_t) len};
            if (!umem) {
                return resolve_return_value::Return(-EFAULT);
            }
            return resolve_return_value::Return(selfRef->handler->write((const void *) umem.Pointer(), len));
        } else {
            return resolve_return_value::Return(-EFAULT);
        }
    });
    return {.result = result.result, .async = result.async};
}

file_descriptor_result
FileDescriptor::writev(ProcThread *process, uintptr_t usersp_iov_ptr, int iovcnt, std::function<void(intptr_t)> func) {
#ifdef WRITEV_IGNORE
#ifdef WRITEV_DEBUG
    std::cout << "writev(..) ignored\n";
#endif
    return {.result = 0, .async = false};
#else
    auto selfRef = this->selfRef.lock();
    std::shared_ptr<writev_state> state{new writev_state};
    state->remaining = iovcnt;
#ifdef WRITEV_DEBUG
    std::cout << "writev: remaining=" << state->remaining << "\n";
#endif
    auto result = process->resolve_read(usersp_iov_ptr, sizeof(iovec) * iovcnt, false, [func] (uintptr_t returnv) mutable {func(returnv);}, [process, selfRef, usersp_iov_ptr, iovcnt, state, func] (bool success, bool async, std::function<void (intptr_t)> asyncFunc) mutable {
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
                auto nestedResult = process->resolve_read((uintptr_t) iov[i].iov_base, iov[i].iov_len, async, asyncFunc, [selfRef, process, umem, iov, iovcnt, i, state, func] (bool success, bool, const std::function<void (uintptr_t)> &) mutable {
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
                            selfRef->handler->write(state->iovecs[i]->iov_base, (intptr_t) state->iovecs[i]->iov_len);
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
#endif
}

bool FileDescriptor::stat(struct stat &st) const {
    return handler->stat(st);
}

bool FileDescriptor::stat(struct statx &st) const {
    return handler->stat(st);
}

intptr_t FileDescriptor::ioctl(callctx &ctx, intptr_t cmd, intptr_t arg) {
    return handler->ioctl(ctx, cmd, arg);
}

int FileDescriptor::readdir(const std::function<bool (kdirent &dirent)> &func) const {
    return handler->readdir(func);
}