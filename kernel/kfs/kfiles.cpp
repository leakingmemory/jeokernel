//
// Created by sigsegv on 6/6/22.
//

#include <iostream>
#include <kfs/kfiles.h>
#include <files/directory.h>
#include <files/symlink.h>
#include <resource/reference.h>
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

class kdirectory_lazy_file : public lazy_kfile, public referrer {
private:
    std::weak_ptr<kdirectory_lazy_file> selfRef{};
    std::shared_ptr<kdirent> dirent;
    std::string name;
private:
    kdirectory_lazy_file(const std::shared_ptr<kdirent> &dirent, const std::string &name) : referrer("kdirectory_lazy_file"), dirent(dirent), name(name) {}
public:
    static std::shared_ptr<kdirectory_lazy_file> Create(const std::shared_ptr<kdirent> &dirent, const std::string &name);
    std::string GetReferrerIdentifier() override;
    reference<kfile> Load(std::shared_ptr<class referrer> &referrer) override;
};

std::shared_ptr<kdirectory_lazy_file>
kdirectory_lazy_file::Create(const std::shared_ptr<kdirent> &dirent, const std::string &name) {
    std::shared_ptr<kdirectory_lazy_file> lz{new kdirectory_lazy_file(dirent, name)};
    std::weak_ptr<kdirectory_lazy_file> w{lz};
    lz->selfRef = w;
    return lz;
}

std::string kdirectory_lazy_file::GetReferrerIdentifier() {
    return name;
}

reference<kfile> kdirectory_lazy_file::Load(std::shared_ptr<class referrer> &referrer) {
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    reference<kfile> file = dirent->File(selfRef);
    if (!reference_is_a<kdirectory_impl>(file)) {

        if (!reference_is_a<ksymlink_impl>(file)) {
            return file.CreateReference(referrer);
        } else {
            reference<ksymlink_impl> symli = reference_dynamic_cast<ksymlink_impl>(std::move(file));
            std::shared_ptr<ksymlink> syml = ksymlink::Create(symli);
            auto symlRef = syml->CreateReference(referrer);
            return reference_dynamic_cast<kfile>(std::move(symlRef));
        }
    } else {
        std::shared_ptr<kdirectory> dir = kdirectory::Create(file, name);
        auto genRef = dir->CreateReference(referrer);
        return genRef;
    }
}

void kdirectory::Init(const std::shared_ptr<kdirectory> &selfRef) {
    std::weak_ptr<kdirectory> weakRef{selfRef};
    this->selfRef = weakRef;
    SetSelfRef(selfRef);
}

void kdirectory::Init(const std::shared_ptr<kdirectory> &selfRef, const reference<kfile> &impl) {
    Init(selfRef);
    std::shared_ptr<class referrer> referrer = selfRef;
    this->impl = impl.CreateReference(referrer);
}

void kdirectory::Init(const std::shared_ptr<kdirectory> &selfRef, const std::shared_ptr<kdirectory_impl> &impl) {
    Init(selfRef);
    std::shared_ptr<class referrer> referrer = selfRef;
    this->impl = impl->CreateReference(referrer);
}

std::shared_ptr<kdirectory> kdirectory::Create(const reference<kfile> &impl, const std::string &name) {
    std::shared_ptr<kdirectory> kdir{new kdirectory(name)};
    kdir->Init(kdir, impl);
    return kdir;
}

std::shared_ptr<kdirectory> kdirectory::Create(const reference<kdirectory_impl> &impl, const std::string &name) {
    std::shared_ptr<kdirectory> kdir{new kdirectory(name)};
    kdir->Init(kdir);
    auto refCopy = impl.CreateReference(kdir);
    kdir->impl = reference_dynamic_cast<kfile>(std::move(refCopy));
    return kdir;
}

std::shared_ptr<kdirectory> kdirectory::Create(const std::shared_ptr<kdirectory_impl> &impl, const std::string &name) {
    std::shared_ptr<kdirectory> kdir{new kdirectory(name)};
    kdir->Init(kdir, impl);
    return kdir;
}

std::string kdirectory::GetReferrerIdentifier() {
    return name;
}

kdirectory *kdirectory::GetResource() {
    return this;
}

kfile_result<std::vector<std::shared_ptr<kdirent>>> kdirectory::Entries() {
    kfile *cwd_file = &(*impl);
    kdirectory_impl *dir = dynamic_cast<kdirectory_impl *>(cwd_file);
    auto impl_entries = dir->Entries();
    if (impl_entries.status != kfile_status::SUCCESS) {
        return {.result = {}, .status = impl_entries.status};
    }
    std::vector<std::shared_ptr<kdirent>> entries{};
    for (auto impl_entry : impl_entries.result) {
        std::shared_ptr<lazy_kfile> lz = kdirectory_lazy_file::Create(impl_entry, name);
        entries.push_back(std::make_shared<kdirent>(impl_entry->Name(), lz));
    }
    return {.result = entries, .status = kfile_status::SUCCESS};
}

kfile_result<reference<kfile>> kdirectory::Resolve(kdirectory *root, std::shared_ptr<class referrer> &referrer, std::string filename, int resolveSymlinks) {
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
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    for (const auto &entry : entriesResult.result) {
        if (component == entry->Name()) {
            auto file = entry->File(selfRef);
            if (file && !filename.empty()) {
                ksymlink *syml;
                while ((syml = dynamic_cast<ksymlink *>(&(*file))) != nullptr && resolveSymlinks > 0) {
                    --resolveSymlinks;
                    auto result = syml->Resolve(root, selfRef);
                    if (result.status != kfile_status::SUCCESS || !result.result) {
                        return result;
                    }
                    // syml = nullptr; // Pointer becomes unsafe:
                    file = std::move(result.result);
                }
            }
            if (!file) {
                return {.result = {}, .status = kfile_status::SUCCESS};
            }
            if (filename.empty()) {
                auto finalRef = file.CreateReference(referrer);
                return {.result = std::move(finalRef), .status = kfile_status::SUCCESS};
            } else {
                kdirectory *dir = dynamic_cast<kdirectory *>(&(*file));
                if (dir == nullptr) {
                    return {.result = {}, .status = kfile_status::NOT_DIRECTORY};
                }
                return dir->Resolve(root, referrer, filename, resolveSymlinks);
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

kfile_result<reference<kdirectory>>
kdirectory::CreateDirectory(std::shared_ptr<class referrer> &referrer, std::string filename, uint16_t mode) {
    auto *impl = dynamic_cast<kdirectory_impl *> (&(*(this->impl)));
    auto newImpl = impl->CreateDirectory(filename, mode);
    if (newImpl.status != kfile_status::SUCCESS) {
        return {.result = {}, .status = newImpl.status};
    }
    if (!newImpl.result) {
        return {.result = {}, .status = kfile_status::IO_ERROR};
    }
    std::shared_ptr<kdirectory> kdir = kdirectory::Create(newImpl.result, newImpl.result->Kpath());
    return {.result = kdir->CreateReference(referrer), .status = kfile_status::SUCCESS};
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

ksymlink::ksymlink(const std::string &name)  : kfile(name), referrer("ksymlink"), resource<ksymlink>(), impl() {
}

std::shared_ptr<ksymlink> ksymlink::Create(const reference<ksymlink_impl> &impl) {
    std::shared_ptr<ksymlink> syml{new ksymlink(impl->Name())};
    syml->Init(syml, impl);
    return syml;
}

void ksymlink::Init(const std::shared_ptr<ksymlink> &selfRef, const reference<ksymlink_impl> &impl) {
    std::weak_ptr<ksymlink> weakPtr{selfRef};
    this->selfRef = weakPtr;
    SetSelfRef(selfRef);
    auto ref = impl->CreateReference(selfRef);
    this->impl = reference_dynamic_cast<ksymlink_impl>(std::move(ref));
}

std::string ksymlink::GetReferrerIdentifier() {
    return name;
}

ksymlink *ksymlink::GetResource() {
    return this;
}

kfile_result<reference<kfile>> ksymlink::Resolve(kdirectory *root, std::shared_ptr<class referrer> &referrer) {
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
            kdir = kdirectory::Create(impl->parent, syml);
            parent = &(*kdir);
        } else {
            parent = dynamic_cast<kdirectory *>(&(*(impl->parent)));
        }
        if (parent == nullptr) {
            return {.result = {}, .status = kfile_status::SUCCESS};
        }
        return parent->Resolve(root, referrer, syml);
    }
    if (root == nullptr) {
        return {.result = {}, .status = kfile_status::SUCCESS};
    }
    return root->Resolve(root, referrer, syml);
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
