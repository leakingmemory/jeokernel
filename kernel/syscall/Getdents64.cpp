//
// Created by sigsegv on 11/18/22.
//

#include "Getdents64.h"
#include "SyscallCtx.h"
#include <errno.h>
#include <iostream>
#include <dirent.h>
#include <exec/procthread.h>

//#define GETDENTS64_DEBUG

class Getdents64_File : public referrer {
private:
    std::weak_ptr<Getdents64_File> selfRef{};
    Getdents64_File() : referrer("Getdents64_File") {}
public:
    static std::shared_ptr<Getdents64_File> Create();
    std::string GetReferrerIdentifier() override;
    void Iteration(dirent64 &de, kdirent &dirent);
};

std::shared_ptr<Getdents64_File> Getdents64_File::Create() {
    std::shared_ptr<Getdents64_File> obj{new Getdents64_File()};
    std::weak_ptr<Getdents64_File> weakPtr{obj};
    obj->selfRef = weakPtr;
    return obj;
}

std::string Getdents64_File::GetReferrerIdentifier() {
    return "";
}

void Getdents64_File::Iteration(dirent64 &de, kdirent &dirent) {
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    auto file = dirent.File(selfRef);
    de.d_ino = file->InodeNum();
    if (dynamic_cast<kdirectory *>(&(*file)) != nullptr) {
        de.d_type = DT_DIR;
    } else {
        de.d_type = DT_REG;
    }
}

int Getdents64::GetDents64(const FileDescriptor &desc, void *dirents, size_t count) {
    size_t remaining{count};
    void *ptr = dirents;
    dirent64 *prev{nullptr};
    bool found{false};
    auto fileReader = Getdents64_File::Create();
    desc.readdir([&ptr, &prev, &remaining, &found, fileReader] (kdirent &dirent) {
        found = true;
        const std::string name{dirent.Name()};
        auto reclen = dirent64::Reclen(name);
        if (reclen > remaining) {
#ifdef GETDENTS64_DEBUG
            std::cout << "getdents64: " << name << " postponed due to length " << std::dec << reclen << ">" << remaining << "\n";
#endif
            return false;
        }
#ifdef GETDENTS64_DEBUG
        std::cout << "getdents64: " << name << " reclen " << reclen << "\n";
#endif
        dirent64 &de = *((dirent64 *) ptr);
        de.d_off = reclen;
        de.d_reclen = reclen;
        strcpy(de.d_name, name.c_str());
        fileReader->Iteration(de, dirent);
        ptr = ((uint8_t *) ptr) + reclen;
        remaining -= reclen;
        prev = &de;
        return true;
    });
    if (prev != nullptr) {
        prev->d_off = 0;
    }
    if (found && remaining == count) {
#ifdef GETDENTS64_DEBUG
        std::cout << "getdents64: need longer buf\n";
#endif
        return -EINVAL;
    }
    auto sz = count - remaining;
#ifdef GETDENTS64_DEBUG
    std::cout << "getdents64: " << std::dec << sz << "\n";
#endif
    return sz;
}

int64_t Getdents64::Call(int64_t fd, int64_t uptr_dirent, int64_t count, int64_t, SyscallAdditionalParams &params) {
    if (uptr_dirent == 0 || count < (offsetof(dirent64, d_name) + 1)) {
        return -EINVAL;
    }
    SyscallCtx ctx{params};
#ifdef GETDENTS64_DEBUG
    std::cout << "getdents64(" << std::dec << fd << ", " << std::hex << uptr_dirent << std::dec << ", " << count << ")\n";
#endif
    auto task_id = get_scheduler()->get_current_task_id();
    auto desc = ctx.GetProcess().get_file_descriptor(fd);
    if (!desc) {
        return -EBADF;
    }
    return ctx.Write(uptr_dirent, count, [this, ctx, task_id, desc, count] (void *dirent) {
        Queue(task_id, [this, ctx, desc, dirent, count] () {
            auto retv = GetDents64(*desc, dirent, count);
            ctx.ReturnWhenNotRunning(retv);
        });
        return resolve_return_value::AsyncReturn();
    });
}