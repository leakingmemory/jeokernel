//
// Created by sigsegv on 11/12/22.
//

#include "Execve.h"
#include "SyscallCtx.h"
#include <errno.h>
#include <string>
#include <iostream>

int64_t Execve::Call(int64_t uptr_filename, int64_t uptr_argv, int64_t uptr_envp, int64_t flags, SyscallAdditionalParams &params) {
    if (uptr_filename == 0 || uptr_argv == 0 || uptr_envp == 0) {
        return -EINVAL;
    }
    SyscallCtx ctx{params};
    return ctx.ReadString(uptr_filename, [ctx, uptr_argv, uptr_envp] (const std::string &rd_filename) mutable {
        std::string filename{rd_filename};
        std::cout << "execve(" << filename << ", ...)\n";
        return ctx.NestedReadNullterminatedArrayOfPointers(uptr_argv, [ctx, filename, uptr_envp] (void **argv_ptr, size_t len) mutable {
            std::cout << "Argv " << len << ":\n";
            return ctx.NestedReadArrayOfStrings(argv_ptr, len, [ctx, filename, uptr_envp] (const std::vector<std::string> &c_argv) mutable {
                std::vector<std::string> argv{c_argv};
                for (auto arg : argv) {
                    std::cout << " - " << arg << "\n";
                }
                return ctx.NestedReadNullterminatedArrayOfPointers(uptr_envp, [ctx, filename, argv] (void **envp, size_t len) mutable {
                    std::cout << "Env " << len << ":\n";
                    return ctx.NestedReadArrayOfStrings(envp, len, [ctx, filename, argv] (const std::vector<std::string> &c_env) {
                        std::vector<std::string> env{c_env};
                        for (auto e : env) {
                            std::cout << " - " << e << "\n";
                        }
                        return resolve_return_value::Return(-EOPNOTSUPP);
                    });
                });
            });
        });
    });
}