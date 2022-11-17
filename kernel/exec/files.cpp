//
// Created by sigsegv on 9/12/22.
//

#include <iostream>
#include <errno.h>
#include <exec/files.h>
#include <kfs/kfiles.h>
#include <mutex>

void FsStat::Stat(kfile &file, struct stat64 &st) {
    st.st_mode = file.Mode();
    st.st_size = file.Size();
    st.st_gid = file.Gid();
    st.st_uid = file.Uid();
    st.st_blksize = file.BlockSize();
    st.st_dev = file.SysDevId();
    st.st_ino = file.InodeNum();

    // TODO
    st.st_blocks = 0;
    st.st_atim = {};
    st.st_ctim = {};
    st.st_mtim = {};
    st.st_nlink = 0;
    st.st_rdev = 0;

    auto *statable = dynamic_cast<kstatable *>(&file);
    if (statable != nullptr) {
        statable->stat(st);
    } else if (dynamic_cast<kdirectory *>(&file) != nullptr) {
        st.st_mode |= S_IFDIR;
    } else {
        st.st_mode |= S_IFREG;
    }
}

FsFileDescriptorHandler::FsFileDescriptorHandler(const FsFileDescriptorHandler &cp) : mtx() {
    std::lock_guard lock{*((hw_spinlock *) &(cp.mtx))};
    this->file = cp.file;
    this->offset = cp.offset;
    this->openRead = cp.openRead;
    this->openWrite = cp.openWrite;
    this->nonblock = cp.nonblock;
}

std::shared_ptr<FileDescriptorHandler> FsFileDescriptorHandler::clone() {
    return std::make_shared<FsFileDescriptorHandler>((const FsFileDescriptorHandler &) *this);
}

std::shared_ptr<kfile> FsFileDescriptorHandler::get_file() {
    return file;
}

bool FsFileDescriptorHandler::can_read() {
    return openRead;
}

resolve_return_value FsFileDescriptorHandler::read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len) {
    if (!openRead) {
        return resolve_return_value::Return(-EINVAL);
    }
    size_t offset{0};
    {
        std::lock_guard lock{mtx};
        offset = this->offset;
    }
    auto result = file->Read(offset, ptr, len);
    if (result.status == kfile_status::SUCCESS || result.result > 0) {
        auto rd = result.result;
        std::unique_lock lock{mtx};
        if (offset == this->offset) {
            if (rd > 0) {
                this->offset += rd;
            }
            return resolve_return_value::Return(rd);
        } else {
            lock.release();
            return read(ctx, ptr, len);
        }
    } else {
        switch (result.status) {
            case kfile_status::IO_ERROR:
                return resolve_return_value::Return(-EIO);
            case kfile_status::NOT_DIRECTORY:
                return resolve_return_value::Return(-ENOTDIR);
            default:
                return resolve_return_value::Return(-EIO);
        }
    }
}

resolve_return_value FsFileDescriptorHandler::read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len, uintptr_t offset) {
    if (!openRead) {
        return resolve_return_value::Return(-EINVAL);
    }
    auto result = file->Read(offset, ptr, len);
    if (result.status == kfile_status::SUCCESS || result.result > 0) {
        return resolve_return_value::Return(result.result);
    } else {
        switch (result.status) {
            case kfile_status::IO_ERROR:
                return resolve_return_value::Return(-EIO);
            case kfile_status::NOT_DIRECTORY:
                return resolve_return_value::Return(-ENOTDIR);
            default:
                return resolve_return_value::Return(-EIO);
        }
    }
}

intptr_t FsFileDescriptorHandler::write(const void *ptr, intptr_t len) {
    if (!openWrite) {
        return -EINVAL;
    }
    std::cerr << "File write: Not implemented\n";
    return -EIO;
}

bool FsFileDescriptorHandler::stat(struct stat64 &st) {
    FsStat::Stat(*file, st);
    return true;
}

intptr_t FsFileDescriptorHandler::ioctl(callctx &ctx, intptr_t cmd, intptr_t arg) {
    std::cout << "fsfile->ioctl(0x" << std::hex << cmd << ", 0x" << arg << std::dec << ")\n";
    return -EOPNOTSUPP;
}

std::shared_ptr<FileDescriptorHandler> FsDirectoryDescriptorHandler::clone() {

}

std::shared_ptr<kfile> FsDirectoryDescriptorHandler::get_file() {
    std::shared_ptr<kfile> f{dir};
    return f;
}

bool FsDirectoryDescriptorHandler::can_read() {
    return true;
}

resolve_return_value FsDirectoryDescriptorHandler::read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len) {
    return resolve_return_value::Return(-EISDIR);
}

resolve_return_value
FsDirectoryDescriptorHandler::read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len, uintptr_t offset) {
    return resolve_return_value::Return(-EISDIR);
}

intptr_t FsDirectoryDescriptorHandler::write(const void *ptr, intptr_t len) {
    return -EISDIR;
}

bool FsDirectoryDescriptorHandler::stat(struct stat64 &st) {
    FsStat::Stat(*dir, st);
    return true;
}

intptr_t FsDirectoryDescriptorHandler::ioctl(callctx &ctx, intptr_t cmd, intptr_t arg) {
    return -EOPNOTSUPP;
}
