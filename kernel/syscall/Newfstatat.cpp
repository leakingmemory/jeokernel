//
// Created by sigsegv on 8/10/22.
//

#include <exec/procthread.h>
#include <exec/usermem.h>
#include <exec/files.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <iostream>
#include "Newfstatat.h"

int64_t Newfstatat::Call(int64_t dfd, int64_t uptr_filename, int64_t uptr_statbuf, int64_t flag, SyscallAdditionalParams &params) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    if (uptr_filename == 0) {
        return -EINVAL;
    }

    constexpr int suppFlags = AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW | AT_SYMLINK_FOLLOW | AT_NO_AUTOMOUNT;
    constexpr int nonsuppFlags = ~suppFlags;

    if ((flag & nonsuppFlags) != 0) {
        return -EINVAL;
    }

    auto result = process->resolve_read_nullterm(uptr_filename, false, [current_task, scheduler] (intptr_t result) {
        scheduler->when_not_running(*current_task, [current_task, result] () {
            current_task->get_cpu_state().rax = (uint64_t) result;
            current_task->set_blocked(false);
        });
    }, [this, process, scheduler, current_task, dfd, uptr_filename, uptr_statbuf, flag] (bool success, bool async, size_t length, std::function<void (intptr_t)> asyncFunc) {
        if (!success) {
            return resolve_return_value::Return(-EFAULT);
        }
        std::string filename{};
        {
            UserMemory userMemory{*process, (uintptr_t) uptr_filename, length};
            filename.append((const char *) userMemory.Pointer(), length);
        }
        auto result = process->resolve_read(uptr_statbuf, sizeof(struct stat), async, asyncFunc, [this, process, scheduler, current_task, dfd, filename, uptr_statbuf, flag] (bool success, bool, std::function<void (intptr_t)> asyncFunc) {
            if (success && !process->resolve_write(uptr_statbuf, sizeof(struct stat))) {
                success = false;
            }
            if (!success) {
                return resolve_return_value::Return(-EFAULT);
            }
            Queue([process, asyncFunc, uptr_statbuf, filename, dfd, flag] () mutable {
                UserMemory stat_mem{*process, (uintptr_t) uptr_statbuf, sizeof(struct stat), true};
                if (!stat_mem) {
                    asyncFunc(-EFAULT);
                    return;
                }
                struct stat st{};

                if ((!filename.empty() && filename.starts_with("/")) || dfd == AT_FDCWD) {
                    // TODO
                    std::cout << "newfstatat(" << std::dec << dfd << ", \"" << filename << "\", " << std::hex
                              << uptr_statbuf << ", " << flag << std::dec << ")\n";
                    auto fileResolve = process->ResolveFile(filename);
                    if (fileResolve.status != kfile_status::SUCCESS) {
                        int err{-EIO};
                        switch (fileResolve.status) {
                            case kfile_status::IO_ERROR:
                                err = -EIO;
                                break;
                            case kfile_status::NOT_DIRECTORY:
                                err = -ENOTDIR;
                                break;
                            default:
                                err = -EIO;
                        }
                        asyncFunc(err);
                        return;
                    }
                    auto file = fileResolve.result;
                    if (!file) {
                        asyncFunc(-ENOENT);
                        return;
                    }
                    FsStat::Stat(*file, st);
                } else {
                    FileDescriptor fd{process->get_file_descriptor(dfd)};
                    if (!fd.Valid()) {
                        asyncFunc(-EBADF);
                        return;
                    }
                    if (filename.empty()) {
                        if ((flag & AT_EMPTY_PATH) != AT_EMPTY_PATH) {
                            asyncFunc(-EINVAL);
                            return;
                        }
                        if (!fd.stat(st)) {
                            asyncFunc(-EBADF);
                            return;
                        }
                        memcpy(stat_mem.Pointer(), &st, sizeof(st));
                        asyncFunc(0);
                        return;
                    }
                    // TODO
                    std::cout << "newfstatat(" << std::dec << dfd << ", \"" << filename << "\", " << std::hex
                              << uptr_statbuf << ", " << flag << std::dec << ")\n";
                    asm("ud2");
                }
                memcpy(stat_mem.Pointer(), &st, sizeof(st));
                asyncFunc(0);
            });
            return resolve_return_value::AsyncReturn();
        });
        return result.async ? resolve_return_value::AsyncReturn() : resolve_return_value::Return(result.result);
    });
    if (result.async) {
        current_task->set_blocked(true);
        params.DoContextSwitch(true);
        return 0;
    } else {
        return result.result;
    }
}