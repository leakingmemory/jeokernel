//
// Created by sigsegv on 9/12/22.
//

#include <iostream>
#include <errno.h>
#include <exec/files.h>
#include <kfs/kfiles.h>
#include <mutex>

void FsStat::Stat(kfile &file, struct stat &st) {
    st.st_mode = file.Mode();
    st.st_size = file.Size();
    st.st_gid = file.Gid();
    st.st_uid = file.Uid();
    st.st_blksize = file.BlockSize();
    st.st_dev = file.SysDevId();
    st.st_ino = file.InodeNum();

    if (dynamic_cast<kdirectory *>(&file) != nullptr) {
        st.st_mode |= S_IFDIR;
    } else {
        st.st_mode |= S_IFREG;
    }

    // TODO
    st.st_blocks = 0;
    st.st_atim = {};
    st.st_ctim = {};
    st.st_mtim = {};
    st.st_nlink = 0;
    st.st_rdev = 0;
}

std::shared_ptr<kfile> FsFileDescriptorHandler::get_file() {
    return file;
}

bool FsFileDescriptorHandler::can_read() {
    return openRead;
}

intptr_t FsFileDescriptorHandler::read(void *ptr, intptr_t len) {
    if (!openRead) {
        return -EINVAL;
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
            return rd;
        } else {
            lock.release();
            return read(ptr, len);
        }
    } else {
        switch (result.status) {
            case kfile_status::IO_ERROR:
                return -EIO;
            case kfile_status::NOT_DIRECTORY:
                return -ENOTDIR;
            default:
                return -EIO;
        }
    }
}

intptr_t FsFileDescriptorHandler::read(void *ptr, intptr_t len, uintptr_t offset) {
    if (!openRead) {
        return -EINVAL;
    }
    auto result = file->Read(offset, ptr, len);
    if (result.status == kfile_status::SUCCESS || result.result > 0) {
        return result.result;
    } else {
        switch (result.status) {
            case kfile_status::IO_ERROR:
                return -EIO;
            case kfile_status::NOT_DIRECTORY:
                return -ENOTDIR;
            default:
                return -EIO;
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

bool FsFileDescriptorHandler::stat(struct stat &st) {
    FsStat::Stat(*file, st);
    return true;
}

int FsFileDescriptorHandler::ioctl(intptr_t cmd, intptr_t arg) {
    std::cout << "fsfile->ioctl(0x" << std::hex << cmd << ", 0x" << arg << std::dec << ")\n";
    return -EOPNOTSUPP;
}