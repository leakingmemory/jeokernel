//
// Created by sigsegv on 6/6/22.
//

#include <iostream>
#include <kfs/kfiles.h>
#include <files/directory.h>
#include <files/symlink.h>

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

struct kmount {
    std::string name;
    std::shared_ptr<directory> rootdir;
};

static std::vector<kmount> mounts{};
static std::shared_ptr<kdirectory_impl> rootdir{new kdirectory_impl({}, "/", {})};

uint32_t kfile::Mode() const {
    return file ? file->Mode() : 0040555;
}

int kfile::Gid() const {
    return 0;
}

int kfile::Uid() const {
    return 0;
}

std::size_t kfile::Size() const {
    return file ? file->Size() : 0;
}

uintptr_t kfile::InodeNum() const {
    return file ? file->InodeNum() : 0;
}

uint32_t kfile::BlockSize() const {
    return file ? file->BlockSize() : 0;
}

uintptr_t kfile::SysDevId() const {
    return file ? file->SysDevId() : 0;
}

kfile_result<std::size_t> kfile::Read(uint64_t offset, void *ptr, std::size_t len) {
    if (!file) {
        return {.result = 0, .status = kfile_status::IO_ERROR};
    }
    auto result = file->Read(offset, ptr, len);
    if (result.size > 0 || result.status == fileitem_status::SUCCESS) {
        return {.result = result.size, .status = kfile_status::SUCCESS};
    } else {
        return {.result = 0, .status = kfile_status::IO_ERROR};
    }
}

kfile_result<filepage_ref> kfile::GetPage(std::size_t pagenum) {
    if (file == nullptr) {
        return {.result = {}, .status = kfile_status::IO_ERROR};
    }
    auto pageResult = file->GetPage(pagenum);
    if (pageResult.page) {
        auto page = pageResult.page;
        auto data = page->Raw();
        filepage_ref ref{data};
        data->initDone();
        return {.result = ref, .status = kfile_status::SUCCESS};
    }
    return {.result = {}, .status = kfile_status::IO_ERROR};
}

kfile_result<std::vector<std::shared_ptr<kdirent>>> kdirectory_impl::Entries(std::shared_ptr<kfile> this_ref) {
    std::shared_ptr<fileitem> listing_ref{file};
    directory *listing;
    if (listing_ref) {
        listing = dynamic_cast<directory *> (&(*listing_ref));
    } else {
        listing = nullptr;
    }
    for (auto mnt : mounts) {
        if (mnt.name == kpath) {
            listing_ref = mnt.rootdir;
            listing = &(*mnt.rootdir);
        }
    }
    std::vector<std::shared_ptr<kdirent>> items{};
    items.push_back(std::make_shared<kdirent>(".", this_ref));
    if (parent) {
        items.push_back(std::make_shared<kdirent>("..", parent));
    } else {
        items.push_back(std::make_shared<kdirent>("..", this_ref));
    }
    if (listing != nullptr) {
        auto entriesResult = listing->Entries();
        if (entriesResult.status != fileitem_status::SUCCESS) {
            return {.result = {}, .status = kfile_status::IO_ERROR};
        }
        for (auto fsent : entriesResult.entries) {
            std::string name = fsent->Name();
            if (name != ".." && name != ".") {
                std::shared_ptr<fileitem> fsfile = fsent->Item();
                directory *fsdir = dynamic_cast<directory *> (&(*fsfile));
                if (fsdir != nullptr) {
                    std::string subpath{kpath};
                    if (subpath[subpath.size() - 1] != '/') {
                        subpath.append("/", 1);
                    }
                    subpath.append(name);
                    std::shared_ptr<kdirectory_impl> dir{new kdirectory_impl(this_ref, subpath, fsfile)};
                    items.push_back(std::make_shared<kdirent>(name, dir));
                } else {
                    class symlink *syml = dynamic_cast<class symlink *>(&(*fsfile));
                    if (syml != nullptr) {
                        std::shared_ptr<ksymlink_impl> symli = std::make_shared<ksymlink_impl>(this_ref, fsfile);
                        items.push_back(std::make_shared<kdirent>(name, symli));
                    } else {
                        std::shared_ptr<kfile> file{new kfile(fsfile)};
                        items.push_back(std::make_shared<kdirent>(name, file));
                    }
                }
            }
        }
    }
    return {.result = items, .status = kfile_status::SUCCESS};
}

void kdirectory_impl::Mount(const std::shared_ptr<directory> &fsroot) {
    mounts.push_back({.name = kpath, .rootdir = fsroot});
}

std::string kdirectory_impl::Kpath() {
    return kpath;
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
        if (dynamic_cast<kdirectory_impl *>(&(*(impl_entry->File()))) == nullptr) {
            std::shared_ptr<ksymlink_impl> symli{impl_entry->File()};
            if (!symli) {
                entries.push_back(impl_entry);
            } else {
                std::shared_ptr<ksymlink> syml = std::make_shared<ksymlink>(symli);
                std::shared_ptr<kdirent> entry = std::make_shared<kdirent>(impl_entry->Name(), syml);
                entries.push_back(entry);
            }
        } else {
            std::shared_ptr<kdirectory> file{new kdirectory(impl_entry->File())};
            std::shared_ptr<kdirent> entry{new kdirent(impl_entry->Name(), file)};
            entries.push_back(entry);
        }
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

void kdirectory::Mount(const std::shared_ptr<directory> &fsroot) {
    auto *impl = dynamic_cast<kdirectory_impl *> (&(*(this->impl)));
    impl->Mount(fsroot);
}

std::string kdirectory::Kpath() {
    auto *impl = dynamic_cast<kdirectory_impl *> (&(*(this->impl)));
    return impl->Kpath();
}

std::shared_ptr<kdirectory> get_kernel_rootdir() {
    return std::make_shared<kdirectory>(rootdir);
}

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
        auto rootimpl = dynamic_cast<kdirectory_impl *>(&(*(impl->parent)));
        if (rootimpl != nullptr) {
            kdir = std::make_shared<kdirectory>(impl->parent);
            root = &(*kdir);
        } else {
            root = dynamic_cast<kdirectory *>(&(*(impl->parent)));
        }
    }
    if (root == nullptr) {
        return {.result = {}, .status = kfile_status::SUCCESS};
    }
    return root->Resolve(root, syml);
}

std::string ksymlink::GetLink() {
    auto *syml = dynamic_cast<class symlink *>(&(*(impl->file)));
    if (syml) {
        return syml->GetLink();
    } else {
        return {};
    }
}
