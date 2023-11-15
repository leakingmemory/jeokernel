//
// Created by sigsegv on 8/10/23.
//

#include "SysOpenImpl.h"
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <exec/procthread.h>
#include <exec/files.h>
#include <resource/reference.h>
#include <resource/referrer.h>
#include <iostream>

//#define DEBUG_OPENAT_CALL

constexpr int supportedOpenFlags = (3 | O_CLOEXEC | O_DIRECTORY | O_NONBLOCK | O_NOFOLLOW | O_LARGEFILE);
constexpr int notSupportedOpenFlags = ~supportedOpenFlags;

class Open_Call : public referrer {
private:
    std::weak_ptr<Open_Call> selfRef{};
    Open_Call() : referrer("Open_Call") {}
public:
    static std::shared_ptr<Open_Call> Create();
    std::string GetReferrerIdentifier() override;
    int Call(ProcThread &proc, const std::string &filename, int flags);
};

std::shared_ptr<Open_Call> Open_Call::Create() {
    std::shared_ptr<Open_Call> openCall{new Open_Call()};
    std::weak_ptr<Open_Call> weakPtr{openCall};
    openCall->selfRef = weakPtr;
    return openCall;
}

std::string Open_Call::GetReferrerIdentifier() {
    return "";
}

int Open_Call::Call(ProcThread &proc, const std::string &filename, int flags) {
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    auto fileResolve = proc.ResolveFile(selfRef, filename);
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
    auto file = std::move(fileResolve.result);
    if (!file) {
#ifdef DEBUG_OPENAT_CALL
        std::cout << "openat: not found: " << filename << "\n";
#endif
        return -ENOENT;
    }

    int followLim = 20;
    while (true) {
        if (!reference_is_a<ksymlink>(file)) {
            break;
        }
        reference<ksymlink> perhapsSymlink = reference_dynamic_cast<ksymlink>(std::move(file));
        if (followLim <= 0 || (flags & O_NOFOLLOW) != 0) {
#ifdef DEBUG_OPENAT_CALL
            std::cout << "openat: symlink nofollow or loop: " << filename << "\n";
#endif
            return -ELOOP;
        }
        --followLim;

        auto rootdir = get_kernel_rootdir();
        auto result = perhapsSymlink->Resolve(&(*rootdir), selfRef);
        if (result.status != kfile_status::SUCCESS) {
#ifdef DEBUG_OPENAT_CALL
            std::cout << "openat: error following symlink: " << filename << "\n";
#endif
            switch (result.status) {
                case kfile_status::IO_ERROR:
                    return -EIO;
                case kfile_status::NOT_DIRECTORY:
                    return -ENOTDIR;
                default:
                    return -EIO;
            }
        }
        file = std::move(result.result);
        if (!file) {
#ifdef DEBUG_OPENAT_CALL
            std::cout << "openat: not found while following symlink for: " << filename << "\n";
#endif
            return -ENOENT;
        }
#ifdef DEBUG_OPENAT_CALL
        std::cout << "openat: symlink followed for: " << filename << "\n";
#endif
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

    if (reference_is_a<kdirectory>(file)) {
        if (openWrite) {
            return -EISDIR;
        }
        reference<kdirectory> perhapsDir = reference_dynamic_cast<kdirectory>(std::move(file));
        std::shared_ptr<FileDescriptorHandler> handler = FsDirectoryDescriptorHandler::Create(perhapsDir);
        auto desc = proc.create_file_descriptor(flags, handler);
#ifdef DEBUG_OPENAT_CALL
        std::cout << "openat -> " << std::dec << desc->FD() << "\n";
#endif
        return desc->FD();
    }

    if ((flags & O_DIRECTORY) != 0) {
        return -ENOTDIR;
    }

    std::shared_ptr<FileDescriptorHandler> handler = FsFileDescriptorHandler::Create(file, openRead, openWrite, (flags & O_NONBLOCK) != 0);
    auto desc = proc.create_file_descriptor(flags, handler);
#ifdef DEBUG_OPENAT_CALL
    std::cout << "openat -> " << std::dec << desc->FD() << "\n";
#endif
    return desc->FD();
}

int SysOpenImpl::DoOpenAt(ProcThread &proc, int dfd, const std::string &filename, int flags, int mode) {
#ifdef DEBUG_OPENAT_CALL
    std::cout << "openat(" << std::dec << dfd << ", " << filename << ", 0x" << std::hex << flags << std::oct << ", 0" << mode << std::dec << ")\n";
#endif
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

    return Open_Call::Create()->Call(proc, filename, flags);
}
