//
// Created by sigsegv on 3/7/23.
//

#include "blockdev_devfs_node.h"
#include <sys/stat.h>

void blockdev_devfs_node::stat(struct statx &st) const {
    st.stx_mode |= S_IFBLK;
}

void blockdev_devfs_node::stat(struct stat64 &st) const {
    st.st_mode |= S_IFBLK;
}