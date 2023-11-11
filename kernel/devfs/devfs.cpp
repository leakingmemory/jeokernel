//
// Created by sigsegv on 9/20/22.
//

#include <devfs/devfs_impl.h>
#include <mutex>
#include <files/fsreference.h>

devfs_node *devfs_node::GetResource() {
    return this;
}

devfs_directory *devfs_directory::GetResource() {
    return this;
}

void devfs_fsresourcelock::lock() {
    mtx.lock();
}

void devfs_fsresourcelock::unlock() {
    mtx.unlock();
}

std::shared_ptr<fsresourcelock> devfs_fsresourcelockfactory::Create() {
    return std::make_shared<devfs_fsresourcelock>();
}

devfs_node_impl::devfs_node_impl(fsresourcelockfactory &lockfactory, uint32_t mode) : devfs_node(lockfactory), mode(mode) {
}

uint32_t devfs_node_impl::Mode() {
    return mode;
}

std::size_t devfs_node_impl::Size() {
    return 0;
}

uintptr_t devfs_node_impl::InodeNum() {
    return 0;
}

uint32_t devfs_node_impl::BlockSize() {
    return 0;
}

uintptr_t devfs_node_impl::SysDevId() {
    return 0;
}

file_getpage_result devfs_node_impl::GetPage(std::size_t pagenum) {
    return {.page = {}, .status = fileitem_status::NOT_SUPPORTED_FS_FEATURE};
}

file_read_result devfs_node_impl::Read(uint64_t offset, void *ptr, std::size_t length) {
    return {.size = 0, .status = fileitem_status::NOT_SUPPORTED_FS_FEATURE};
}

devfs_directory_impl::devfs_directory_impl(fsresourcelockfactory &lockfactory, uint32_t mode) : devfs_directory(lockfactory), fsreferrer("devfs_directory_impl"), mtx(), entries(), mode(mode) {
}

std::shared_ptr<devfs_directory_impl> devfs_directory_impl::Create(uint32_t mode) {
    devfs_fsresourcelockfactory lockfactory{};
    std::shared_ptr<devfs_directory_impl> impl{new devfs_directory_impl(lockfactory, mode)};
    std::weak_ptr<devfs_directory_impl> w{impl};
    impl->SetSelfRef(impl);
    impl->selfRef = w;
    return impl;
}

std::string devfs_directory_impl::GetReferrerIdentifier() {
    return "";
}

uint32_t devfs_directory_impl::Mode() {
    return mode;
}

std::size_t devfs_directory_impl::Size() {
    return 0;
}

uintptr_t devfs_directory_impl::InodeNum() {
    return 0;
}

uint32_t devfs_directory_impl::BlockSize() {
    return 0;
}

uintptr_t devfs_directory_impl::SysDevId() {
    return 0;
}

file_getpage_result devfs_directory_impl::GetPage(std::size_t pagenum) {
    return {.page = {}, .status = fileitem_status::INVALID_REQUEST};
}

file_read_result devfs_directory_impl::Read(uint64_t offset, void *ptr, std::size_t length) {
    return {.size = 0, .status = fileitem_status::INVALID_REQUEST};
}

entries_result devfs_directory_impl::Entries() {
    entries_result result{.entries = {}, .status = fileitem_status::SUCCESS};
    {
        std::lock_guard lock{mtx};
        for (const auto &entry: entries) {
            result.entries.emplace_back(entry);
        }
    }
    return result;
}

class devfs_directory_entry : public directory_entry {
private:
    std::weak_ptr<devfs_node> file{};
public:
    devfs_directory_entry(const std::string &name, const std::shared_ptr<devfs_node> &fileitem) : directory_entry(name), file(fileitem) {}
    directory_entry_result LoadItem(const std::shared_ptr<fsreferrer> &referrer) override;
};

directory_entry_result devfs_directory_entry::LoadItem(const std::shared_ptr<fsreferrer> &referrer) {
    std::shared_ptr<devfs_node> node = file.lock();
    fsreference<fileitem> fileitem;
    if (node) {
        fsreference<devfs_node> genRef = node->CreateReference(referrer);
        fileitem = fsreference_dynamic_cast<class fileitem>(std::move(genRef));
    }
    if (fileitem) {
        return {.file = std::move(fileitem), .status = fileitem_status::SUCCESS};
    } else {
        return {.file = {}, .status = fileitem_status::IO_ERROR};
    }
}

void devfs_directory_impl::Add(const std::string &name, std::shared_ptr<devfs_node> node) {
    std::lock_guard lock{mtx};
    auto iterator = entries.begin();
    while (iterator != entries.end()) {
        auto ld = (*iterator)->LoadItem(selfRef.lock());
        if (!ld.file) {
            entries.erase(iterator);
            return;
        }
        ++iterator;
    }
    entries.emplace_back(std::make_shared<devfs_directory_entry>(name, node));
}

void devfs_directory_impl::Remove(std::shared_ptr<devfs_node> node) {
    std::lock_guard lock{mtx};
    auto iterator = entries.begin();
    while (iterator != entries.end()) {
        auto ld = (*iterator)->LoadItem(selfRef.lock());
        if (!ld.file || (&(*(ld.file))) == dynamic_cast<fileitem *>(&(*(node)))) {
            entries.erase(iterator);
            return;
        }
        ++iterator;
    }
}

devfs::devfs() : root(devfs_directory_impl::Create(00755)){
}

std::shared_ptr<devfs_directory> devfs::GetRoot() {
    return root;
}

static hw_spinlock devfsLock{};
static std::shared_ptr<devfs> devfsSingleton{};

std::shared_ptr<devfs> GetDevfs() {
    std::lock_guard lock{devfsLock};
    if (!devfsSingleton) {
        devfsSingleton = std::make_shared<devfs>();
    }
    return devfsSingleton;
}
