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
#include <resource/referrer.h>
#include <resource/reference.h>

//#define DEBUG_EXECVE

class Execve_Call : public referrer {
private:
    std::weak_ptr<Execve_Call> selfRef{};
    Execve_Call() : referrer("Execve_Call") {}
public:
    static std::shared_ptr<Execve_Call> Create();
    std::string GetReferrerIdentifier() override;
    void Call(SyscallCtx &ctx, const std::string &filename, const std::vector<std::string> &argv, const std::vector<std::string> &env);
};

std::shared_ptr<Execve_Call> Execve_Call::Create() {
    std::shared_ptr<Execve_Call> e{new Execve_Call()};
    std::weak_ptr<Execve_Call> w{e};
    e->selfRef = w;
    return e;
}

std::string Execve_Call::GetReferrerIdentifier() {
    return "";
}

void Execve_Call::Call(SyscallCtx &ctx, const std::string &filename, const std::vector<std::string> &argv, const std::vector<std::string> &env) {
#ifdef DEBUG_EXECVE
    std::cout << "Exec: " << filename;
    for (const auto &arg : argv) {
        std::cout << " " << arg;
    }
    std::cout << "\n";
#endif
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    auto binary = ctx.GetProcess().ResolveFile(selfRef, filename);
    if (binary.status != kfile_status::SUCCESS) {
        ctx.ReturnWhenNotRunning(-EIO);
        return;
    }
    int linkLimit = 20;
    while (binary.result) {
        if (!reference_is_a<ksymlink>(binary.result)) {
            break;
        }
        reference<ksymlink> symlink = reference_dynamic_cast<ksymlink>(std::move(binary.result));
        if (linkLimit == 0) {
            ctx.ReturnWhenNotRunning(-ELOOP);
            return;
        }
        --linkLimit;
        auto rootdir = get_kernel_rootdir();
        binary = symlink->Resolve(&(*rootdir), selfRef);
        if (binary.status != kfile_status::SUCCESS) {
            ctx.ReturnWhenNotRunning(-EIO);
            return;
        }
    }
    if (!binary.result) {
        ctx.ReturnWhenNotRunning(-ENOENT);
        return;
    }
    ctx.GetProcess().GetProcess()->TearDownMemory();
    auto cwd = ctx.GetProcess().GetCwd(selfRef);
    reference<kdirectory> cwd_dir = reference_dynamic_cast<kdirectory>(std::move(cwd));
    std::shared_ptr<tty> tty{};
    std::shared_ptr<Exec> exec = Exec::Create(tty, cwd, *cwd_dir, binary.result, filename, argv, env, 0);
    auto result = exec->RunFromExistingProcess(&(ctx.GetProcess()), [ctx] (bool success, const ExecStartVector &startVector) {
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
}

int64_t Execve::Call(int64_t uptr_filename, int64_t uptr_argv, int64_t uptr_envp, int64_t flags, SyscallAdditionalParams &params) {
    if (uptr_filename == 0 || uptr_argv == 0 || uptr_envp == 0) {
        return -EINVAL;
    }
    SyscallCtx ctx{params, "Execve"};
    auto task_id = params.TaskId();
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
                            Execve_Call::Create()->Call(ctx, filename, argv, env);
                        });
                        return resolve_return_value::AsyncReturn();
                    });
                });
            });
        });
    });
}