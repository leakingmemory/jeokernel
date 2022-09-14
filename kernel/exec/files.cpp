//
// Created by sigsegv on 9/12/22.
//

#include <iostream>
#include <errno.h>
#include <exec/files.h>
#include <kfs/kfiles.h>

void FsStat::Stat(kfile &file, struct stat &st) {
    st.st_mode = file.Mode();
    st.st_size = file.Size();
    st.st_gid = file.Gid();
    st.st_uid = file.Uid();

    if (dynamic_cast<kdirectory *>(&file) != nullptr) {
        st.st_mode |= S_IFDIR;
    } else {
        st.st_mode |= S_IFREG;
    }

    // TODO
    st.st_blksize = 0;
    st.st_blocks = 0;
    st.st_atim = {};
    st.st_ctim = {};
    st.st_mtim = {};
    st.st_dev = 0;
    st.st_ino = 0;
    st.st_nlink = 0;
    st.st_rdev = 0;
}

intptr_t FsFileDescriptorHandler::write(const void *ptr, intptr_t len) {
    std::cerr << "File write: Not implemented\n";
    return -EIO;
}

bool FsFileDescriptorHandler::stat(struct stat &st) {
    FsStat::Stat(*file, st);
    return true;
}