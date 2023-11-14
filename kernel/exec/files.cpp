//
// Created by sigsegv on 9/12/22.
//

#include <iostream>
#include <errno.h>
#include <exec/files.h>
#include <kfs/kfiles.h>
#include <mutex>
#include <resource/reference.h>
#include <resource/referrer.h>

void FsStat::Stat(const kfile &file, struct stat64 &st) {
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

    auto *statable = dynamic_cast<const kstatable *>(&file);
    if (statable != nullptr) {
        statable->stat(st);
    } else if (dynamic_cast<const kdirectory *>(&file) != nullptr) {
        st.st_mode |= S_IFDIR;
    } else {
        st.st_mode |= S_IFREG;
    }
}

void FsStat::Stat(const kfile &file, statx &st) {
    st.stx_mode = file.Mode();
    st.stx_size = file.Size();
    st.stx_gid = file.Gid();
    st.stx_uid = file.Uid();
    st.stx_blksize = file.BlockSize();
    st.stx_dev_major = file.SysDevId();
    st.stx_dev_minor = 0;
    st.stx_ino = file.InodeNum();

    // TODO
    st.stx_blocks = 0;
    st.stx_atime = {};
    st.stx_btime = {};
    st.stx_ctime = {};
    st.stx_mtime = {};
    st.stx_nlink = 0;
    st.stx_rdev_major = 0;
    st.stx_rdev_minor = 0;

    auto *statable = dynamic_cast<const kstatable *>(&file);
    if (statable != nullptr) {
        statable->stat(st);
    } else if (dynamic_cast<const kdirectory *>(&file) != nullptr) {
        st.stx_mode |= S_IFDIR;
    } else {
        st.stx_mode |= S_IFREG;
    }
}

FsFileDescriptorHandler::FsFileDescriptorHandler(const FsFileDescriptorHandler &cp) : FileDescriptorHandler(), referrer("FsFileDescriptorHandler") {
    std::lock_guard lock{*((hw_spinlock *) &(cp.mtx))};
    CopyFrom(cp);
    this->offset = cp.offset;
    this->openRead = cp.openRead;
    this->openWrite = cp.openWrite;
    this->nonblock = cp.nonblock;
}

std::shared_ptr<FsFileDescriptorHandler>
FsFileDescriptorHandler::Create(const reference<kfile> &file, bool openRead, bool openWrite, bool nonblock) {
    std::shared_ptr<FsFileDescriptorHandler> handler{new FsFileDescriptorHandler(openRead, openWrite, nonblock)};
    std::weak_ptr<FsFileDescriptorHandler> weakPtr{handler};
    handler->selfRef = weakPtr;
    handler->file = file.CreateReference(handler);
    return handler;
}

std::shared_ptr<FsFileDescriptorHandler> FsFileDescriptorHandler::Create(const FsFileDescriptorHandler &cp) {
    std::shared_ptr<FsFileDescriptorHandler> handler{new FsFileDescriptorHandler(cp)};
    std::weak_ptr<FsFileDescriptorHandler> weakPtr{handler};
    handler->selfRef = weakPtr;
    handler->file = cp.file.CreateReference(handler);
    return handler;
}

std::string FsFileDescriptorHandler::GetReferrerIdentifier() {
    return "";
}

std::shared_ptr<FileDescriptorHandler> FsFileDescriptorHandler::clone() {
    return FsFileDescriptorHandler::Create((const FsFileDescriptorHandler &) *this);
}

reference<kfile> FsFileDescriptorHandler::get_file(std::shared_ptr<class referrer> &referrer) {
    return file.CreateReference(referrer);
}

bool FsFileDescriptorHandler::can_seek() {
    return true;
}

bool FsFileDescriptorHandler::can_read() {
    return openRead;
}

intptr_t FsFileDescriptorHandler::seek(intptr_t offset, SeekWhence whence) {
    std::lock_guard lock{mtx};
    int64_t off = whence == SeekWhence::SET ? 0 : (whence == SeekWhence::CUR ? this->offset : file->Size());
    off += offset;
    if (off >= 0) {
        if (off < file->Size()) {
            this->offset = off;
            return 0;
        }
    }
    return -EINVAL;
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

bool FsFileDescriptorHandler::stat(struct stat64 &st) const {
    FsStat::Stat(*file, st);
    return true;
}

bool FsFileDescriptorHandler::stat(struct statx &st) const {
    FsStat::Stat(*file, st);
    return true;
}

intptr_t FsFileDescriptorHandler::ioctl(callctx &ctx, intptr_t cmd, intptr_t arg) {
    std::cout << "fsfile->ioctl(0x" << std::hex << cmd << ", 0x" << arg << std::dec << ")\n";
    return -EOPNOTSUPP;
}

int FsFileDescriptorHandler::readdir(const std::function<bool (kdirent &dirent)> &) {
    return -ENOTDIR;
}

FsDirectoryDescriptorHandler::FsDirectoryDescriptorHandler(const FsDirectoryDescriptorHandler &cp) : FileDescriptorHandler(),
                                                                                                     referrer("FsDirectoryDescriptorHandler"), dir(), readdirInited(false), dirents(), iterator() {
    std::lock_guard lock{*((hw_spinlock *) &(cp.mtx))};
    CopyFrom(cp);
    readdirInited = cp.readdirInited;
    dirents = cp.dirents;
    iterator = dirents.begin();
    if (cp.iterator) {
        auto iterator = dirents.begin();
        while (iterator != dirents.end()) {
            if (*iterator == *(cp.iterator)) {
                this->iterator = iterator;
                break;
            }
            ++iterator;
        }
    }
}

std::shared_ptr<FsDirectoryDescriptorHandler> FsDirectoryDescriptorHandler::Create(const reference<kdirectory> &reference) {
    std::shared_ptr<FsDirectoryDescriptorHandler> handler{new FsDirectoryDescriptorHandler()};
    std::weak_ptr<FsDirectoryDescriptorHandler> weakPtr{handler};
    handler->selfRef = weakPtr;
    handler->dir = reference.CreateReference(handler);
    return handler;
}

std::string FsDirectoryDescriptorHandler::GetReferrerIdentifier() {
    return "";
}

std::shared_ptr<FileDescriptorHandler> FsDirectoryDescriptorHandler::clone() {
    std::shared_ptr<FsDirectoryDescriptorHandler> handler{new FsDirectoryDescriptorHandler((const FsDirectoryDescriptorHandler &) *this)};
    std::weak_ptr<FsDirectoryDescriptorHandler> weakPtr{handler};
    handler->selfRef = weakPtr;
    handler->dir = dir.CreateReference(handler);
    return handler;
}

reference<kfile> FsDirectoryDescriptorHandler::get_file(std::shared_ptr<class referrer> &referrer) {
    auto dirRef = dir->CreateReference(referrer);
    return reference_dynamic_cast<kfile>(std::move(dirRef));
}

bool FsDirectoryDescriptorHandler::can_seek() {
    return false;
}

bool FsDirectoryDescriptorHandler::can_read() {
    return true;
}

intptr_t FsDirectoryDescriptorHandler::seek(intptr_t offset, SeekWhence whence) {
    return -EINVAL;
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

bool FsDirectoryDescriptorHandler::stat(struct stat64 &st) const {
    FsStat::Stat(*dir, st);
    return true;
}

bool FsDirectoryDescriptorHandler::stat(struct statx &st) const {
    FsStat::Stat(*dir, st);
    return true;
}

intptr_t FsDirectoryDescriptorHandler::ioctl(callctx &ctx, intptr_t cmd, intptr_t arg) {
    return -EOPNOTSUPP;
}

int FsDirectoryDescriptorHandler::readdir(const std::function<bool (kdirent &dirent)> &c_accept) {
    std::lock_guard lock{mtx};
    if (!readdirInited) {
        auto result = dir->Entries();
        if (result.status != kfile_status::SUCCESS) {
            return -EIO;
        }
        dirents = result.result;
        iterator = dirents.begin();
        readdirInited = true;
    }
    int count{0};
    std::function<bool (kdirent &)> accept{c_accept};
    while (iterator != dirents.end() && accept(**iterator)) {
        ++iterator;
        ++count;
    }
    return count;
}
