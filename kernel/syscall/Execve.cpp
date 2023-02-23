//
// Created by sigsegv on 11/12/22.
//

#include "Execve.h"
#include "SyscallCtx.h"
#include <errno.h>
#include <string>
#include <iostream>
#include <exec/exec.h>
#include <exec/procthread.h>

//#define DEBUG_EXECVE

int64_t Execve::Call(int64_t uptr_filename, int64_t uptr_argv, int64_t uptr_envp, int64_t flags, SyscallAdditionalParams &params) {
    if (uptr_filename == 0 || uptr_argv == 0 || uptr_envp == 0) {
        return -EINVAL;
    }
    SyscallCtx ctx{params};
    auto task_id = get_scheduler()->get_current_task_id();
    return ctx.ReadString(uptr_filename, [this, ctx, task_id, uptr_argv, uptr_envp] (const std::string &rd_filename) mutable {
        std::string filename{rd_filename};
#ifdef DEBUG_EXECVE
        std::cout << "execve(" << filename << ", ...)\n";
#endif
        return ctx.NestedReadNullterminatedArrayOfPointers(uptr_argv, [this, ctx, task_id, filename, uptr_envp] (void **argv_ptr, size_t len) mutable {
#ifdef DEBUG_EXECVE
            std::cout << "Argv " << len << ":\n";
#endif
            return ctx.NestedReadArrayOfStrings(argv_ptr, len, [this, ctx, task_id, filename, uptr_envp] (const std::vector<std::string> &c_argv) mutable {
                std::vector<std::string> argv{c_argv};
#ifdef DEBUG_EXECVE
                for (auto arg : argv) {
                    std::cout << " - " << arg << "\n";
                }
#endif
                return ctx.NestedReadNullterminatedArrayOfPointers(uptr_envp, [this, ctx, task_id, filename, argv] (void **envp, size_t len) mutable {
#ifdef DEBUG_EXECVE
                    std::cout << "Env " << len << ":\n";
#endif
                    return ctx.NestedReadArrayOfStrings(envp, len, [this, ctx, task_id, filename, argv] (const std::vector<std::string> &c_env) {
                        std::vector<std::string> env{c_env};
#ifdef DEBUG_EXECVE
                        for (auto e : env) {
                            std::cout << " - " << e << "\n";
                        }
#endif
                        Queue(task_id, [ctx, filename, argv, env] () mutable {
                            auto binary = ctx.GetProcess().ResolveFile(filename);
                            if (binary.status != kfile_status::SUCCESS) {
                                ctx.ReturnWhenNotRunning(-EIO);
                                return;
                            }
                            if (!binary.result) {
                                ctx.ReturnWhenNotRunning(-ENOENT);
                                return;
                            }
                            ctx.GetProcess().GetProcess()->TearDownMemory();
                            auto cwd = ctx.GetProcess().GetCwd();
                            std::shared_ptr<kdirectory> cwd_dir{cwd};
                            std::shared_ptr<tty> tty{};
                            Exec exec{tty, cwd, *cwd_dir, binary.result, filename, argv, env, 0};
                            auto result = exec.Run(&(ctx.GetProcess()), [ctx] (bool success, const ExecStartVector &startVector) {
                                if (!success) {
                                    ctx.KillAsync();
                                    return;
                                }
                                ctx.EntrypointAsync(startVector.entrypoint, startVector.fsBase, startVector.stackAddr);
                            });
                            if (result != ExecResult::DETACHED) {
                                if (result == ExecResult::SOFTERROR) {
                                    ctx.ReturnWhenNotRunning(-EINVAL);
                                }
                                ctx.KillAsync();
                            }
                        });
                        return resolve_return_value::AsyncReturn();
                    });
                });
            });
        });
    });
}