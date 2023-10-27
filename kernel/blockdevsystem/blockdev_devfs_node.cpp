//
// Created by sigsegv on 3/7/23.
//

#include <core/blockdev_devfs_node.h>
#include <sys/stat.h>
#include <devfs/devfs_impl.h>

class blockdev_devfs_node_impl : public devfs_node_impl, public kstatable, public blockdev_devfs_node {
public:
    blockdev_devfs_node_impl(const std::shared_ptr<blockdev> &bdev) : devfs_node_impl(00600), blockdev_devfs_node(bdev) {}
    void stat(struct stat64 &st) const override;
    void stat(struct statx &st) const override;
};

std::shared_ptr<blockdev_devfs_node> blockdev_devfs_node::Create(const std::shared_ptr<blockdev> &bdev) {
    auto ref = std::make_shared<blockdev_devfs_node_impl>(bdev);
    std::weak_ptr<blockdev_devfs_node_impl> impl{ref};
    ref->impl = impl;
    return ref;
}

std::shared_ptr<blockdev_devfs_node> blockdev_devfs_node::FromFileItem(const std::shared_ptr<fileitem> &fitem) {
    return std::dynamic_pointer_cast<blockdev_devfs_node>(fitem);
}

std::shared_ptr<fileitem> blockdev_devfs_node::AsFileitem() {
    return this->impl.lock();
}

void blockdev_devfs_node_impl::stat(struct statx &st) const {
    st.stx_mode |= S_IFBLK;
}

void blockdev_devfs_node_impl::stat(struct stat64 &st) const {
    st.st_mode |= S_IFBLK;
}
