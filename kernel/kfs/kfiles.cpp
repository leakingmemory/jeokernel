//
// Created by sigsegv on 6/6/22.
//

#include <iostream>
#include <kfs/kfiles.h>
#include <files/directory.h>
#include <files/symlink.h>
#include <concurrency/hw_spinlock.h>
#include <mutex>
#include "kdirectory_impl.h"
#include "ksymlink_impl.h"

std::string text(kfile_status status) {
    switch (status) {
        case kfile_status::SUCCESS:
            return "Success";
        case kfile_status::IO_ERROR:
            return "Input/output error";
        case kfile_status::NOT_DIRECTORY:
            return "Not a directory";
        default:
            return "Unknown error";
    }
}

std::string kfile::Name() const {
    return name;
}

int kfile::Gid() const {
    return 0;
}

int kfile::Uid() const {
    return 0;
}

class kdirectory_lazy_file : public lazy_kfile {
private:
    std::shared_ptr<kdirent> dirent;
    std::string name;
public:
    kdirectory_lazy_file(const std::shared_ptr<kdirent> &dirent, const std::string &name) : dirent(dirent), name(name) {}
    std::shared_ptr<kfile> Load() override;
};

std::shared_ptr<kfile> kdirectory_lazy_file::Load() {
    auto file = dirent->File();
    if (dynamic_cast<kdirectory_impl *>(&(*file)) == nullptr) {
        std::shared_ptr<ksymlink_impl> symli = std::dynamic_pointer_cast<ksymlink_impl>(file);
        if (!symli) {
            return file;
        } else {
            std::shared_ptr<ksymlink> syml = std::make_shared<ksymlink>(symli);
            return syml;
        }
    } else {
        std::shared_ptr<kdirectory> dir{new kdirectory(file, name)};
        return dir;
    }
    return {};
}

kfile_result<std::vector<std::shared_ptr<kdirent>>> kdirectory::Entries() {
    kfile *cwd_file = &(*impl);
    kdirectory_impl *dir = dynamic_cast<kdirectory_impl *>(cwd_file);
    auto impl_entries = dir->Entries(impl);
    if (impl_entries.status != kfile_status::SUCCESS) {
        return {.result = {}, .status = impl_entries.status};
    }
    std::vector<std::shared_ptr<kdirent>> entries{};
    for (auto impl_entry : impl_entries.result) {
        std::shared_ptr<lazy_kfile> lz = std::make_shared<kdirectory_lazy_file>(impl_entry, name);
        entries.push_back(std::make_shared<kdirent>(impl_entry->Name(), lz));
    }
    return {.result = entries, .status = kfile_status::SUCCESS};
}

kfile_result<std::shared_ptr<kfile>> kdirectory::Resolve(kdirectory *root, std::string filename, int resolveSymlinks) {
    if (filename.empty() || filename.starts_with("/")) {
        return {.result = {}, .status = kfile_status::SUCCESS};
    }
    std::string component{};
    {
        std::size_t count;
        do {
            if (filename.empty()) {
                return {.result = {}, .status = kfile_status::SUCCESS};
            }
            component.clear();
            count = 0;
            {
                auto iterator = filename.begin();
                while (iterator != filename.end() && *iterator != '/') {
                    ++count;
                    ++iterator;
                }
                component.append(filename.c_str(), count);
                while (iterator != filename.end() && *iterator == '/') {
                    ++count;
                    ++iterator;
                }
            }
            std::string remaining{};
            remaining.append(filename.c_str() + count);
            filename = remaining;
        } while (component == "." && !filename.empty());
    }
    auto entriesResult = Entries();
    if (entriesResult.status != kfile_status::SUCCESS) {
        return {.result = {}, .status = kfile_status::IO_ERROR};
    }
    for (const auto &entry : entriesResult.result) {
        if (component == entry->Name()) {
            auto file = entry->File();
            if (!filename.empty()) {
                ksymlink *syml;
                while ((syml = dynamic_cast<ksymlink *>(&(*file))) != nullptr && resolveSymlinks > 0) {
                    --resolveSymlinks;
                    auto result = syml->Resolve(root);
                    if (result.status != kfile_status::SUCCESS || !result.result) {
                        return result;
                    }
                    // syml = nullptr; // Pointer becomes unsafe:
                    file = result.result;
                }
            }
            if (filename.empty()) {
                return {.result = file, .status = kfile_status::SUCCESS};
            } else {
                kdirectory *dir = dynamic_cast<kdirectory *>(&(*file));
                if (dir == nullptr) {
                    return {.result = {}, .status = kfile_status::NOT_DIRECTORY};
                }
                return dir->Resolve(root, filename, resolveSymlinks);
            }
        }
    }
    return {.result = {}, .status = kfile_status::SUCCESS};
}

std::shared_ptr<filesystem> kdirectory::Unmount() {
    auto *impl = dynamic_cast<kdirectory_impl *> (&(*(this->impl)));
    return impl->Unmount();
}

void kdirectory::Mount(const std::string &devname, const std::string &fstype, const std::string &mntopts, const std::shared_ptr<filesystem> &fs) {
    auto *impl = dynamic_cast<kdirectory_impl *> (&(*(this->impl)));
    impl->Mount(devname, fstype, mntopts, fs);
}

std::string kdirectory::Kpath() {
    auto *impl = dynamic_cast<kdirectory_impl *> (&(*(this->impl)));
    return impl->Kpath();
}

fileitem *kdirectory::GetFileitem() const {
    return impl->GetFileitem();
}

uint32_t kdirectory::Mode() const {
    return impl->Mode();
}
std::size_t kdirectory::Size() const {
    return impl->Size();
}
uintptr_t kdirectory::InodeNum() const {
    return impl->InodeNum();
}
uint32_t kdirectory::BlockSize() const {
    return impl->BlockSize();
}
uintptr_t kdirectory::SysDevId() const {
    return impl->SysDevId();
}
kfile_result<std::size_t> kdirectory::Read(uint64_t offset, void *ptr, std::size_t len) {
    return impl->Read(offset, ptr, len);
}
kfile_result<filepage_ref> kdirectory::GetPage(std::size_t pagenum) {
    return impl->GetPage(pagenum);
}

ksymlink::ksymlink(const std::shared_ptr<ksymlink_impl> &impl)  : kfile(impl->Name()), impl(impl) {}

kfile_result<std::shared_ptr<kfile>> ksymlink::Resolve(kdirectory *root) {
    std::string syml = GetLink();
    std::shared_ptr<kdirectory> kdir{};
    if (syml.starts_with("/")) {
        std::string remaining{};
        do {
            remaining.append(syml.c_str() + 1, syml.size() - 1);
            syml = remaining;
            remaining.clear();
        } while (syml.starts_with("/"));
    } else {
        auto parentimpl = dynamic_cast<kdirectory_impl *>(&(*(impl->parent)));
        kdirectory *parent;
        if (parentimpl != nullptr) {
            kdir = std::make_shared<kdirectory>(impl->parent, syml);
            parent = &(*kdir);
        } else {
            parent = dynamic_cast<kdirectory *>(&(*(impl->parent)));
        }
        if (parent == nullptr) {
            return {.result = {}, .status = kfile_status::SUCCESS};
        }
        return parent->Resolve(root, syml);
    }
    if (root == nullptr) {
        return {.result = {}, .status = kfile_status::SUCCESS};
    }
    return root->Resolve(root, syml);
}

std::string ksymlink::GetLink() {
    auto *syml = dynamic_cast<class symlink *>(impl->GetFileitem());
    if (syml) {
        return syml->GetLink();
    } else {
        return {};
    }
}

fileitem *ksymlink::GetFileitem() const {
    return impl->GetFileitem();
}

uint32_t ksymlink::Mode() const {
    return impl->Mode();
}
std::size_t ksymlink::Size() const {
    return impl->Size();
}
uintptr_t ksymlink::InodeNum() const {
    return impl->InodeNum();
}
uint32_t ksymlink::BlockSize() const {
    return impl->BlockSize();
}
uintptr_t ksymlink::SysDevId() const {
    return impl->SysDevId();
}
kfile_result<std::size_t> ksymlink::Read(uint64_t offset, void *ptr, std::size_t len) {
    return impl->Read(offset, ptr, len);
}
kfile_result<filepage_ref> ksymlink::GetPage(std::size_t pagenum) {
    return impl->GetPage(pagenum);
}
