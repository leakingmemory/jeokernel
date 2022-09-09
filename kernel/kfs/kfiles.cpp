//
// Created by sigsegv on 6/6/22.
//

#include <iostream>
#include <kfs/kfiles.h>
#include <files/directory.h>

struct kmount {
    std::string name;
    std::shared_ptr<directory> rootdir;
};

static std::vector<kmount> mounts{};
static std::shared_ptr<kdirectory_impl> rootdir{new kdirectory_impl({}, "/", {})};

uint32_t kfile::Mode() {
    return file ? file->Mode() : 0040555;
}

int kfile::Gid() {
    return 0;
}

int kfile::Uid() {
    return 0;
}

std::size_t kfile::Size() {
    return file ? file->Size() : 0;
}

std::size_t kfile::Read(uint64_t offset, void *ptr, std::size_t len) {
    return file ? file->Read(offset, ptr, len) : 0;
}

filepage_ref kfile::GetPage(std::size_t pagenum) {
    if (file == nullptr) {
        return {};
    }
    auto page = file->GetPage(pagenum);
    auto data = page->Raw();
    filepage_ref ref{data};
    data->initDone();
    return ref;
}

std::vector<std::shared_ptr<kdirent>> kdirectory_impl::Entries(std::shared_ptr<kfile> this_ref) {
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
        for (auto fsent : listing->Entries()) {
            std::string name = fsent->Name();
            std::shared_ptr<fileitem> fsfile = fsent->Item();
            directory *fsdir = dynamic_cast<directory *> (&(*fsfile));
            if (fsdir != nullptr && name != ".." && name != ".") {
                std::string subpath{kpath};
                if (subpath[subpath.size() - 1] != '/') {
                    subpath.append("/", 1);
                }
                subpath.append(name);
                std::shared_ptr<kdirectory_impl> dir{new kdirectory_impl(this_ref, subpath, fsfile)};
                items.push_back(std::make_shared<kdirent>(name, dir));
            } else {
                std::shared_ptr<kfile> file{new kfile(fsfile)};
                items.push_back(std::make_shared<kdirent>(name, file));
            }
        }
    }
    return items;
}

void kdirectory_impl::Mount(const std::shared_ptr<directory> &fsroot) {
    mounts.push_back({.name = kpath, .rootdir = fsroot});
}

std::string kdirectory_impl::Kpath() {
    return kpath;
}

std::vector<std::shared_ptr<kdirent>> kdirectory::Entries() {
    kfile *cwd_file = &(*impl);
    kdirectory_impl *dir = dynamic_cast<kdirectory_impl *>(cwd_file);
    auto impl_entries = dir->Entries(impl);
    std::vector<std::shared_ptr<kdirent>> entries{};
    for (auto impl_entry : impl_entries) {
        if (dynamic_cast<kdirectory_impl *>(&(*(impl_entry->File()))) == nullptr) {
            entries.push_back(impl_entry);
        } else {
            std::shared_ptr<kdirectory> file{new kdirectory(impl_entry->File())};
            std::shared_ptr<kdirent> entry{new kdirent(impl_entry->Name(), file)};
            entries.push_back(entry);
        }
    }
    return entries;
}

std::shared_ptr<kfile> kdirectory::Resolve(std::string filename) {
    if (filename.empty() || filename.starts_with("/")) {
        return {};
    }
    std::string component{};
    {
        std::size_t count;
        do {
            if (filename.empty()) {
                return {};
            }
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
    for (auto entry : Entries()) {
        if (component == entry->Name()) {
            if (filename.empty()) {
                return entry->File();
            } else {
                kdirectory *dir = dynamic_cast<kdirectory *>(&(*(entry->File())));
                if (dir == nullptr) {
                    return {};
                }
                return dir->Resolve(filename);
            }
        }
    }
    return {};
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
