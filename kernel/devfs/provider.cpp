//
// Created by sigsegv on 9/21/22.
//

#include <devfs/devfs.h>
#include <devfs/devfsinit.h>
#include <filesystems/filesystem.h>

class devfs_filesystem : public filesystem {
public:
    filesystem_get_node_result<fsreference<directory>> GetRootDirectory(std::shared_ptr<filesystem> shared_this, const std::shared_ptr<fsreferrer> &referrer) override;
};

class devfs_provider : public special_filesystem_provider {
public:
    std::string name() const override;
    std::shared_ptr<filesystem> open() const override;
};

filesystem_get_node_result<fsreference<directory>> devfs_filesystem::GetRootDirectory(std::shared_ptr<filesystem> shared_this, const std::shared_ptr<fsreferrer> &referrer) {
    std::shared_ptr<fsresource<devfs_directory>> devfsRoot = GetDevfs()->GetRoot();
    auto genRef = devfsRoot->CreateReference(referrer);
    auto dirRef = fsreference_dynamic_cast<directory>(std::move(genRef));
    return {.node = std::move(dirRef), .status = filesystem_status::SUCCESS};
}

std::string devfs_provider::name() const {
    return "devfs";
}

std::shared_ptr<filesystem> devfs_provider::open() const {
    return std::make_shared<devfs_filesystem>();
}

void InstallDevfs() {
    add_filesystem_provider(std::make_shared<devfs_provider>());
}
