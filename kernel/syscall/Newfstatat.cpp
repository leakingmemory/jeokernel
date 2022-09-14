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

    current_task->set_blocked(true);
    params.DoContextSwitch(true);
    process->resolve_read_nullterm(uptr_filename, [this, process, scheduler, current_task, dfd, uptr_filename, uptr_statbuf, flag] (bool success, size_t length) {
        if (!success) {
            scheduler->when_not_running(*current_task, [current_task] () {
                current_task->get_cpu_state().rax = (uint64_t) -EFAULT;
                current_task->set_blocked(false);
            });
            return;
        }
        std::string filename{};
        {
            UserMemory userMemory{*process, (uintptr_t) uptr_filename, length};
            filename.append((const char *) userMemory.Pointer(), length);
        }
        process->resolve_read(uptr_statbuf, sizeof(struct stat), [this, process, scheduler, current_task, dfd, filename, uptr_statbuf, flag] (bool success) {
            if (success && !process->resolve_write(uptr_statbuf, sizeof(struct stat))) {
                success = false;
            }
            if (!success) {
                scheduler->when_not_running(*current_task, [current_task] () {
                    current_task->get_cpu_state().rax = (uint64_t) -EFAULT;
                    current_task->set_blocked(false);
                });
                return;
            }
            Queue([process, uptr_statbuf, scheduler, current_task, filename, dfd, flag] () {
                UserMemory stat_mem{*process, (uintptr_t) uptr_statbuf, sizeof(struct stat), true};
                if (!stat_mem) {
                    scheduler->when_not_running(*current_task, [current_task]() {
                        current_task->get_cpu_state().rax = (uint64_t) -EFAULT;
                        current_task->set_blocked(false);
                    });
                    return;
                }
                struct stat st{};

                if ((!filename.empty() && filename.starts_with("/")) || dfd == AT_FDCWD) {
                    // TODO
                    std::cout << "newfstatat(" << std::dec << dfd << ", \"" << filename << "\", " << std::hex
                              << uptr_statbuf << ", " << flag << std::dec << ")\n";
                    auto file = process->ResolveFile(filename);
                    if (!file) {
                        scheduler->when_not_running(*current_task, [current_task]() {
                            current_task->get_cpu_state().rax = (uint64_t) -ENOENT;
                            current_task->set_blocked(false);
                        });
                        return;
                    }
                    FsStat::Stat(*file, st);
                } else {
                    FileDescriptor fd{process->get_file_descriptor(dfd)};
                    if (!fd.Valid()) {
                        scheduler->when_not_running(*current_task, [current_task]() {
                            current_task->get_cpu_state().rax = (uint64_t) -EBADF;
                            current_task->set_blocked(false);
                        });
                        return;
                    }
                    if (filename.empty()) {
                        if ((flag & AT_EMPTY_PATH) != AT_EMPTY_PATH) {
                            scheduler->when_not_running(*current_task, [current_task]() {
                                current_task->get_cpu_state().rax = (uint64_t) -EINVAL;
                                current_task->set_blocked(false);
                            });
                            return;
                        }
                        if (!fd.stat(st)) {
                            scheduler->when_not_running(*current_task, [current_task]() {
                                current_task->get_cpu_state().rax = (uint64_t) -EBADF;
                                current_task->set_blocked(false);
                            });
                            return;
                        }
                        memcpy(stat_mem.Pointer(), &st, sizeof(st));
                        scheduler->when_not_running(*current_task, [current_task]() {
                            current_task->get_cpu_state().rax = (uint64_t) 0;
                            current_task->set_blocked(false);
                        });
                        return;
                    }
                    // TODO
                    std::cout << "newfstatat(" << std::dec << dfd << ", \"" << filename << "\", " << std::hex
                              << uptr_statbuf << ", " << flag << std::dec << ")\n";
                    asm("ud2");
                }
                memcpy(stat_mem.Pointer(), &st, sizeof(st));
                scheduler->when_not_running(*current_task, [current_task]() {
                    current_task->get_cpu_state().rax = (uint64_t) 0;
                    current_task->set_blocked(false);
                });
            });
        });
    });
    return 0;
}