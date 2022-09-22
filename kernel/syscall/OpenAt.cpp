//
// Created by sigsegv on 9/9/22.
//

#include "OpenAt.h"
#include <exec/procthread.h>
#include <exec/usermem.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <kfs/kfiles.h>
#include <exec/files.h>

constexpr int supportedOpenFlags = (3 | O_CLOEXEC | O_NONBLOCK);
constexpr int notSupportedOpenFlags = ~supportedOpenFlags;

int OpenAt::DoOpenAt(ProcThread &proc, int dfd, const std::string &filename, int flags, int mode) {
    std::cout << "openat(" << std::dec << dfd << ", " << filename << ", 0x" << std::hex << flags << std::oct << ", 0" << mode << std::dec << ")\n";
    if (dfd != AT_FDCWD) {
        std::cerr << "not implemented: open at fd\n";
        return -EIO;
    }

    if ((flags & 3) == 3) {
        return -EINVAL;
    }

    {
        int notSupportedFlags = flags & notSupportedOpenFlags;
        if (notSupportedFlags != 0) {
            std::cerr << "not implemented: open flags << " << std::hex << notSupportedFlags << std::dec << "\n";
            return -EIO;
        }
    }

    auto fileResolve = proc.ResolveFile(filename);
    if (fileResolve.status != kfile_status::SUCCESS) {
        switch (fileResolve.status) {
            case kfile_status::IO_ERROR:
                return -EIO;
            case kfile_status::NOT_DIRECTORY:
                return -ENOTDIR;
            default:
                return -EIO;
        }
    }
    auto file = fileResolve.result;
    if (!file) {
        std::cout << "openat: not found: " << filename << "\n";
        return -ENOENT;
    }

    auto fileMode = file->Mode();
    auto uid = proc.getuid();
    auto gid = proc.getgid();
    if (gid == file->Gid() || uid == 0) {
        fileMode |= (fileMode >> 3) & 7;
    }
    if (uid == file->Uid() || uid == 0) {
        fileMode |= (fileMode >> 6) & 7;
    }

    bool openRead;
    bool openWrite;
    {
        auto modeFlags = flags & 3;
        switch (modeFlags) {
            case O_RDONLY:
                openRead = true;
                openWrite = false;
                break;
            case O_WRONLY:
                openRead = false;
                openWrite = true;
                break;
            case O_RDWR:
                openRead = true;
                openWrite = true;
                break;
            default:
                return -EINVAL;
        }
    }

    if (openRead) {
        if ((fileMode & R_OK) != R_OK) {
            return -EPERM;
        }
    }
    if (openWrite) {
        if ((fileMode & W_OK) != W_OK) {
            return -EPERM;
        }
    }

    std::shared_ptr<FileDescriptorHandler> handler{new FsFileDescriptorHandler(file, openRead, openWrite, (flags & O_NONBLOCK) != 0)};
    FileDescriptor desc = proc.create_file_descriptor(handler);
    std::cout << "openat -> " << std::dec << desc.FD() << "\n";
    return desc.FD();
}

int64_t OpenAt::Call(int64_t dfd, int64_t uptr_filename, int64_t flags, int64_t mode, SyscallAdditionalParams &params) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    current_task->set_blocked(true);
    params.DoContextSwitch(true);
    process->resolve_read_nullterm(uptr_filename, [this, scheduler, current_task, process, uptr_filename, dfd, flags, mode] (bool success, size_t slen) {
        if (!success) {
            scheduler->when_not_running(*current_task, [current_task]() {
                current_task->get_cpu_state().rax = (uint64_t) -EFAULT;
                current_task->set_blocked(false);
            });
            return;
        }
        std::string filename{};
        {
            UserMemory filename_mem{*process, (uintptr_t) uptr_filename, slen};
            filename.append((const char *) filename_mem.Pointer(), slen);
        }
        Queue([this, scheduler, current_task, process, filename, dfd, flags, mode] () {
            auto res = DoOpenAt(*process, dfd, filename, flags, mode);
            scheduler->when_not_running(*current_task, [current_task, res]() {
                current_task->get_cpu_state().rax = (uint64_t) res;
                current_task->set_blocked(false);
            });
        });
    });
    return 0;
}