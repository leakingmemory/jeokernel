//
// Created by sigsegv on 8/10/23.
//

#include "SysOpenImpl.h"
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <exec/procthread.h>
#include <exec/files.h>
#include <iostream>

//#define DEBUG_OPENAT_CALL

constexpr int supportedOpenFlags = (3 | O_CLOEXEC | O_DIRECTORY | O_NONBLOCK | O_NOFOLLOW | O_LARGEFILE);
constexpr int notSupportedOpenFlags = ~supportedOpenFlags;

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
#ifdef DEBUG_OPENAT_CALL
        std::cout << "openat: not found: " << filename << "\n";
#endif
        return -ENOENT;
    }

    int followLim = 20;
    while (true) {
        std::shared_ptr<ksymlink> perhapsSymlink = std::dynamic_pointer_cast<ksymlink>(file);
        if (!perhapsSymlink) {
            break;
        }
        if (followLim <= 0 || (flags & O_NOFOLLOW) != 0) {
#ifdef DEBUG_OPENAT_CALL
            std::cout << "openat: symlink nofollow or loop: " << filename << "\n";
#endif
            return -ELOOP;
        }
        --followLim;

        auto rootdir = get_kernel_rootdir();
        auto result = perhapsSymlink->Resolve(&(*rootdir));
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
        file = result.result;
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

    std::shared_ptr<kdirectory> perhapsDir = std::dynamic_pointer_cast<kdirectory>(file);
    if (perhapsDir) {
        if (openWrite) {
            return -EISDIR;
        }
        std::shared_ptr<FileDescriptorHandler> handler{new FsDirectoryDescriptorHandler(perhapsDir)};
        FileDescriptor desc = proc.create_file_descriptor(flags, handler);
#ifdef DEBUG_OPENAT_CALL
        std::cout << "openat -> " << std::dec << desc.FD() << "\n";
#endif
        return desc.FD();
    }

    if ((flags & O_DIRECTORY) != 0) {
        return -ENOTDIR;
    }

    std::shared_ptr<FileDescriptorHandler> handler{new FsFileDescriptorHandler(file, openRead, openWrite, (flags & O_NONBLOCK) != 0)};
    FileDescriptor desc = proc.create_file_descriptor(flags, handler);
#ifdef DEBUG_OPENAT_CALL
    std::cout << "openat -> " << std::dec << desc.FD() << "\n";
#endif
    return desc.FD();
}