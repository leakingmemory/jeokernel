//
// Created by sigsegv on 8/13/23.
//

#include "SysFaccessatImpl.h"
#include <exec/procthread.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <resource/reference.h>

constexpr int supportedFlags = (AT_EACCESS | AT_SYMLINK_NOFOLLOW);
constexpr int notSupportedFlags = ~supportedFlags;

std::shared_ptr<SysFaccessatImpl> SysFaccessatImpl::Create() {
    std::shared_ptr<SysFaccessatImpl> sysFaccessat{new SysFaccessatImpl()};
    std::weak_ptr<SysFaccessatImpl> weakPtr{sysFaccessat};
    sysFaccessat->selfRef = weakPtr;
    return sysFaccessat;
}

std::string SysFaccessatImpl::GetReferrerIdentifier() {
    return "";
}

int SysFaccessatImpl::DoFaccessat(ProcThread &proc, int dfd, std::string filename, int mode, int flags) {
    constexpr int allAccess = F_OK | X_OK | W_OK | R_OK;
    if ((mode & allAccess) != mode || (flags & notSupportedFlags) != 0 || filename.empty()) {
        return -EINVAL;
    }
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    kfile_result<reference<kfile>> fileResolve{};
    if (dfd == AT_FDCWD || filename.starts_with("/")) {
        fileResolve = proc.ResolveFile(selfRef, filename);
    } else {
        auto fdesc = proc.get_file_descriptor(dfd);
        auto handler = fdesc.GetHandler();
        auto file = handler->get_file(selfRef);
        reference<kdirectory> dir = reference_dynamic_cast<kdirectory>(std::move(file));
        if (!dir) {
            return -ENOTDIR;
        }
        auto rootdir = get_kernel_rootdir();
        fileResolve = dir->Resolve(&(*rootdir), selfRef, filename);
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
            if (!reference_is_a<ksymlink>(fileResolve.result)) {
                break;
            }
            reference<ksymlink> symlink = reference_dynamic_cast<ksymlink>(std::move(fileResolve.result));
            if (linkLimit <= 0) {
                return -ELOOP;
            }
            --linkLimit;
            auto rootdir = get_kernel_rootdir();
            fileResolve = symlink->Resolve(&(*rootdir), selfRef);
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
    auto file = std::move(fileResolve.result);
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