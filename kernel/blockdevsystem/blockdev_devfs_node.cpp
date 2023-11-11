//
// Created by sigsegv on 3/7/23.
//

#include <core/blockdev_devfs_node.h>
#include <sys/stat.h>
#include <devfs/devfs_impl.h>

class blockdev_devfs_node_impl : public devfs_node_impl, public kstatable, public blockdev_devfs_node {
private:
    blockdev_devfs_node_impl(fsresourcelockfactory &lockfactory, const std::shared_ptr<blockdev> &bdev) : devfs_node_impl(lockfactory, 00600), blockdev_devfs_node(bdev) {}
public:
    static std::shared_ptr<blockdev_devfs_node_impl> Create(const std::shared_ptr<blockdev> &bdev);
    void stat(struct stat64 &st) const override;
    void stat(struct statx &st) const override;
};


std::shared_ptr<blockdev_devfs_node_impl> blockdev_devfs_node_impl::Create(const std::shared_ptr<blockdev> &bdev) {
    devfs_fsresourcelockfactory lockfactory{};
    std::shared_ptr<blockdev_devfs_node_impl> impl{new blockdev_devfs_node_impl(lockfactory, bdev)};
    impl->SetSelfRef(impl);
    return impl;
}

std::shared_ptr<blockdev_devfs_node> blockdev_devfs_node::Create(const std::shared_ptr<blockdev> &bdev) {
    auto ref = blockdev_devfs_node_impl::Create(bdev);
    std::weak_ptr<blockdev_devfs_node_impl> impl{ref};
    ref->impl = impl;
    return ref;
}

blockdev_devfs_node *blockdev_devfs_node::FromFileItem(fileitem *fitem) {
    return dynamic_cast<blockdev_devfs_node *>(fitem);
}

std::shared_ptr<fileitem> blockdev_devfs_node::AsFileitem() {
    return this->impl.lock();
}

std::shared_ptr<devfs_node> blockdev_devfs_node::AsDevfsNode() {
    return this->impl.lock();
}

void blockdev_devfs_node_impl::stat(struct statx &st) const {
    st.stx_mode |= S_IFBLK;
}

void blockdev_devfs_node_impl::stat(struct stat64 &st) const {
    st.st_mode |= S_IFBLK;
}
