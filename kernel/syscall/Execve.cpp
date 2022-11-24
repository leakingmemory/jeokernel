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

int64_t Execve::Call(int64_t uptr_filename, int64_t uptr_argv, int64_t uptr_envp, int64_t flags, SyscallAdditionalParams &params) {
    if (uptr_filename == 0 || uptr_argv == 0 || uptr_envp == 0) {
        return -EINVAL;
    }
    SyscallCtx ctx{params};
    return ctx.ReadString(uptr_filename, [this, ctx, uptr_argv, uptr_envp] (const std::string &rd_filename) mutable {
        std::string filename{rd_filename};
        std::cout << "execve(" << filename << ", ...)\n";
        return ctx.NestedReadNullterminatedArrayOfPointers(uptr_argv, [this, ctx, filename, uptr_envp] (void **argv_ptr, size_t len) mutable {
            std::cout << "Argv " << len << ":\n";
            return ctx.NestedReadArrayOfStrings(argv_ptr, len, [this, ctx, filename, uptr_envp] (const std::vector<std::string> &c_argv) mutable {
                std::vector<std::string> argv{c_argv};
                for (auto arg : argv) {
                    std::cout << " - " << arg << "\n";
                }
                return ctx.NestedReadNullterminatedArrayOfPointers(uptr_envp, [this, ctx, filename, argv] (void **envp, size_t len) mutable {
                    std::cout << "Env " << len << ":\n";
                    return ctx.NestedReadArrayOfStrings(envp, len, [this, ctx, filename, argv] (const std::vector<std::string> &c_env) {
                        std::vector<std::string> env{c_env};
                        for (auto e : env) {
                            std::cout << " - " << e << "\n";
                        }
                        Queue([ctx, filename, argv, env] () mutable {
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