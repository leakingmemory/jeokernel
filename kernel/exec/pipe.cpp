//
// Created by sigsegv on 8/22/23.
//

#include <exec/pipe.h>
#include <mutex>
#include <errno.h>
#include <exec/callctx.h>
#include <resource/reference.h>

PipeEnd::~PipeEnd() {
    auto other = otherEnd.lock();
    if (other) {
        {
            std::lock_guard lock{other->mtx};
            other->eof = true;
        }
        other->Notify();
    }
}

void PipeEnd::AddFd(const std::shared_ptr<PipeDescriptorHandler> &fd) {
    std::weak_ptr<PipeDescriptorHandler> weakPtr{fd};
    fds.push_back(weakPtr);
}

void PipeEnd::Notify() {
    while (true) {
        bool readReady1;
        std::vector<std::shared_ptr<PipeDescriptorHandler>> handlers{};
        {
            std::lock_guard lock{mtx};
            {
                auto iterator = readQueue.begin();
                while (iterator != readQueue.end()) {
                    if (iterator->len <= buffer.size()) {
                        iterator->func();
                        readQueue.erase(iterator);
                    } else {
                        break;
                    }
                }
            }
            readReady1 = !buffer.empty() && readQueue.empty();
            auto iterator = fds.begin();
            while (iterator != fds.end()) {
                std::shared_ptr<PipeDescriptorHandler> sh = iterator->lock();
                if (sh) {
                    ++iterator;
                } else {
                    fds.erase(iterator);
                }
            }
        }
        for (const auto &fd : handlers) {
            fd->SetReadyRead(readReady1);
        }
        {
            std::lock_guard lock{mtx};
            auto readReady2 = !buffer.empty() && readQueue.empty();
            if (readReady1 == readReady2) {
                break;
            }
        }
    }
}

void PipeDescriptorHandler::CreateHandlerPair(std::shared_ptr<PipeDescriptorHandler> &a,
                                              std::shared_ptr<PipeDescriptorHandler> &b) {
    std::shared_ptr<PipeEnd> end1 = std::make_shared<PipeEnd>();
    std::shared_ptr<PipeEnd> end2 = std::make_shared<PipeEnd>(end1);
    {
        std::weak_ptr<PipeEnd> weakPtrEnd1{end1};
        end2->otherEnd = weakPtrEnd1;
    }
    a = CreateHandler(end1);
    b = CreateHandler(end2);
}

void PipeDescriptorHandler::Notify() {
    end->Notify();
}

std::shared_ptr<FileDescriptorHandler> PipeDescriptorHandler::clone() {
    return CreateHandler(end);
}

reference<kfile> PipeDescriptorHandler::get_file(std::shared_ptr<class referrer> &referrer) {
    return {};
}

bool PipeDescriptorHandler::can_seek() {
    return false;
}

bool PipeDescriptorHandler::can_read() {
    return true;
}

intptr_t PipeDescriptorHandler::seek(intptr_t offset, SeekWhence whence) {
    return -EOPNOTSUPP;
}

resolve_return_value PipeDescriptorHandler::read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len) {
    auto endpoint = this->end;
    std::lock_guard lock{endpoint->mtx};
    if (!endpoint->readQueue.empty() || (endpoint->buffer.size() < len && !endpoint->eof)) {
        auto ref = self_ref.lock();
        PipeQueuedRead q{.func = [ctx, ref, endpoint, ptr, len] () mutable {
            if (len > endpoint->buffer.size()) {
                len = endpoint->buffer.size();
            }
            memcpy(ptr, endpoint->buffer.data(), len);
            while (len > 0) {
                int eraseLen;
                if (len > std::numeric_limits<int>::max()) {
                    eraseLen = std::numeric_limits<int>::max();
                } else {
                    eraseLen = (int) len;
                }
                endpoint->buffer.erase(0, eraseLen);
                len -= eraseLen;
            }
            ctx->AsyncCtx()->returnAsync(len);
        }, .len = len};
        endpoint->readQueue.push_back(q);
        return resolve_return_value::AsyncReturn();
    }
    if (len > endpoint->buffer.size()) {
        len = endpoint->buffer.size();
    }
    memcpy(ptr, endpoint->buffer.data(), len);
    while (len > 0) {
        int eraseLen;
        if (len > std::numeric_limits<int>::max()) {
            eraseLen = std::numeric_limits<int>::max();
        } else {
            eraseLen = (int) len;
        }
        endpoint->buffer.erase(0, eraseLen);
        len -= eraseLen;
    }
    return resolve_return_value::Return(len);
}

resolve_return_value
PipeDescriptorHandler::read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len, uintptr_t offset) {
    if (offset != 0) {
        return resolve_return_value::Return(-EOPNOTSUPP);
    }
    return read(ctx, ptr, len);
}

intptr_t PipeDescriptorHandler::write(const void *ptr, intptr_t len) {
    std::shared_ptr<PipeEnd> otherEnd = end->otherEnd.lock();
    if (!otherEnd) {
        /* broken pipe - TODO - kill */
        return -EIO;
    }
    {
        std::lock_guard lock{otherEnd->mtx};
        otherEnd->buffer.append((const char *) ptr, len);
    }
    otherEnd->Notify();
    return len;
}

bool PipeDescriptorHandler::stat(struct stat64 &st) const {
    return false;
}

bool PipeDescriptorHandler::stat(struct statx &st) const {
    return false;
}

intptr_t PipeDescriptorHandler::ioctl(callctx &ctx, intptr_t cmd, intptr_t arg) {
    return -EOPNOTSUPP;
}

int PipeDescriptorHandler::readdir(const std::function<bool(kdirent &)> &) {
    return -EOPNOTSUPP;
}
