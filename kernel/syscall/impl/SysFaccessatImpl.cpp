//
// Created by sigsegv on 8/13/23.
//

#include "SysFaccessatImpl.h"
#include <exec/procthread.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

constexpr int supportedFlags = (AT_EACCESS | AT_SYMLINK_NOFOLLOW);
constexpr int notSupportedFlags = ~supportedFlags;

int SysFaccessatImpl::DoFaccessat(ProcThread &proc, int dfd, std::string filename, int mode, int flags) {
    constexpr int allAccess = F_OK | X_OK | W_OK | R_OK;
    if ((mode & allAccess) != mode || (flags & notSupportedFlags) != 0 || filename.empty()) {
        return -EINVAL;
    }
    kfile_result<std::shared_ptr<kfile>> fileResolve{};
    if (dfd == AT_FDCWD || filename.starts_with("/")) {
        fileResolve = proc.ResolveFile(filename);
    } else {
        auto fdesc = proc.get_file_descriptor(dfd);
        auto handler = fdesc.GetHandler();
        auto file = handler->get_file();
        std::shared_ptr<kdirectory> dir = std::dynamic_pointer_cast<kdirectory>(file);
        if (!dir) {
            return -ENOTDIR;
        }
        auto rootdir = get_kernel_rootdir();
        fileResolve = dir->Resolve(&(*rootdir), filename);
    }
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
    if ((flags & AT_SYMLINK_NOFOLLOW) == 0) {
        int linkLimit = 20;
        while (true) {
            std::shared_ptr<ksymlink> symlink = std::dynamic_pointer_cast<ksymlink>(fileResolve.result);
            if (!symlink) {
                break;
            }
            if (linkLimit <= 0) {
                return -ELOOP;
            }
            --linkLimit;
            auto rootdir = get_kernel_rootdir();
            fileResolve = symlink->Resolve(&(*rootdir));
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
        }
    }
    auto file = fileResolve.result;
    if (!file) {
        return -ENOENT;
    }
    int m = file->Mode();
    if ((m & mode) == mode) {
        return 0;
    }
    mode = mode & ~m;
    m = m >> 3;
    {
        auto gid = (flags & AT_EACCESS) == 0 ? proc.getgid() : proc.getegid();
        auto f_gid = file->Gid();
        if (gid == f_gid) {
            if ((m & mode) == mode) {
                return 0;
            }
            mode = mode & ~m;
        }
    }
    m = m >> 3;
    {
        auto uid = (flags & AT_EACCESS) == 0 ? proc.getuid() : proc.geteuid();
        auto f_uid = file->Uid();
        if (uid == f_uid || uid == 0) {
            if ((m & mode) == mode) {
                return 0;
            }
        }
    }
    return -EACCES;
}