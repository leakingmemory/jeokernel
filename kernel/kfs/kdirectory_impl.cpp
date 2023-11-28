//
// Created by sigsegv on 11/9/23.
//

#include <mutex>
#include <concurrency/hw_spinlock.h>
#include <files/directory.h>
#include <kfs/kfiles.h>
#include <filesystems/filesystem.h>
#include "kdirectory_impl.h"
#include "kdirectory_impl_ref.h"
#include "kdirectory_impl_lazy.h"
#include <iostream>

struct kmount {
    kmount_info info;
    std::shared_ptr<filesystem> fs;
};

static hw_spinlock mounts_lock{};
static std::vector<kmount> mounts{};
static std::shared_ptr<kdirectory_impl> rootdir = kdirectory_impl::CreateRoot();

std::vector<kmount_info> GetKmounts() {
    std::vector<kmount_info> kmounts{};
    {
        std::lock_guard lock{mounts_lock};
        for (const auto &mnt: mounts) {
            kmounts.emplace_back(mnt.info);
        }
    }
    return kmounts;
}

std::shared_ptr<kdirectory> get_kernel_rootdir() {
    return kdirectory::Create(rootdir, "/");
}

void kdirectory_impl::Mount(const std::string &devname, const std::string &fstype, const std::string &mntopts, const std::shared_ptr<filesystem> &fs) {
    std::lock_guard lock{mounts_lock};
    mounts.push_back({.info = {.devname = devname, .fstype = fstype, .mntopts = mntopts, .name = Name()}, .fs = fs});
}

std::string kdirectory_impl::Kpath() {
    return Name();
}

kfile_result<std::shared_ptr<kdirectory_impl>> kdirectory_impl::CreateDirectory(std::string filename, uint16_t mode) {
    std::shared_ptr<kdirectory_impl> selfRef = this->selfRef.lock();
    std::shared_ptr<class fsreferrer> referrer = selfRef;

    std::shared_ptr<filesystem> fs{};
    {
        std::lock_guard lock{mounts_lock};
        for (auto mnt: mounts) {
            if (mnt.info.name == Name()) {
                fs = mnt.fs;
            }
        }
    }

    fsreference<directory> dirref{};

    if (fs) {
        auto result = fs->GetRootDirectory(fs, selfRef);
        if (result.status != filesystem_status::SUCCESS || !result.node) {
            switch (result.status) {
                case filesystem_status::SUCCESS:
                case filesystem_status::IO_ERROR:
                case filesystem_status::INTEGRITY_ERROR:
                case filesystem_status::NOT_SUPPORTED_FS_FEATURE:
                case filesystem_status::INVALID_REQUEST:
                    return {.result = {}, .status = kfile_status::IO_ERROR};
                case filesystem_status::NO_AVAIL_INODES:
                    return {.result = {}, .status = kfile_status::NO_AVAIL_INODES};
                case filesystem_status::NO_AVAIL_BLOCKS:
                    return {.result = {}, .status = kfile_status::NO_AVAIL_BLOCKS};
                default:
                    return {.result = {}, .status = kfile_status::IO_ERROR};
            }
        }
        dirref = std::move(result.node);
    } else {
        auto fileref = file.CreateReference(referrer);
        dirref = fsreference_dynamic_cast<directory>(std::move(fileref));
        if (!dirref) {
            return {.result = {}, .status = kfile_status::NOT_DIRECTORY};
        }
    }
    auto mkdirResult = dirref->CreateDirectory(referrer, filename, mode);
    if (mkdirResult.status != fileitem_status::SUCCESS || !mkdirResult.file) {
        switch (mkdirResult.status) {
            case fileitem_status::SUCCESS:
            case fileitem_status::IO_ERROR:
            case fileitem_status::INTEGRITY_ERROR:
            case fileitem_status::NOT_SUPPORTED_FS_FEATURE:
            case fileitem_status::INVALID_REQUEST:
                return {.result = {}, .status = kfile_status::IO_ERROR};
            case fileitem_status::TOO_MANY_LINKS:
                return {.result = {}, .status = kfile_status::TOO_MANY_LINKS};
            case fileitem_status::NO_AVAIL_INODES:
                return {.result = {}, .status = kfile_status::NO_AVAIL_INODES};
            case fileitem_status::NO_AVAIL_BLOCKS:
                return {.result = {}, .status = kfile_status::NO_AVAIL_BLOCKS};
            case fileitem_status::EXISTS:
                return {.result = {}, .status = kfile_status::EXISTS};
            default:
                return {.result = {}, .status = kfile_status::IO_ERROR};
        }
    }
    std::string fullpath{selfRef->Name()};
    fullpath.append("/");
    fullpath.append(filename);
    return {.result = kdirectory_impl::Create(mkdirResult.file, selfRef, fullpath), .status = kfile_status::SUCCESS};
}

kdirectory_impl::kdirectory_impl(const std::string &kpath) : kfile_impl(kpath), parent() {}

void kdirectory_impl::Init(std::shared_ptr<kdirectory_impl> &selfRef, const std::shared_ptr<kdirectory_impl> &parent) {
    std::weak_ptr<kdirectory_impl> weak{selfRef};
    this->selfRef = weak;
    SetSelfRef(selfRef);
    if (parent) {
        this->parent = parent->CreateReference(selfRef);
    }
}

void kdirectory_impl::Init(std::shared_ptr<kdirectory_impl> &selfRef, const reference<kdirectory_impl> &parent) {
    std::weak_ptr<kdirectory_impl> weak{selfRef};
    this->selfRef = weak;
    SetSelfRef(selfRef);
    if (parent) {
        this->parent = parent->CreateReference(selfRef);
    }
}

std::shared_ptr<kdirectory_impl>
kdirectory_impl::Create(const fsreference<fileitem> &file, const std::shared_ptr<kdirectory_impl> &parent, const std::string &kpath) {
    auto dir = kfile_impl::Create<kdirectory_impl>(file, kpath);
    dir->Init(dir, parent);
    return dir;
}

std::shared_ptr<kdirectory_impl>
kdirectory_impl::Create(const fsreference<fileitem> &file, const reference<kdirectory_impl> &parent, const std::string &kpath) {
    auto dir = kfile_impl::Create<kdirectory_impl>(file, kpath);
    dir->Init(dir, parent);
    return dir;
}

std::shared_ptr<kdirectory_impl> kdirectory_impl::CreateRoot() {
    auto dir = kfile_impl::Create<kdirectory_impl>({}, "/");
    std::shared_ptr<kdirectory_impl> parent{};
    dir->Init(dir, parent);
    return dir;
}

kfile_result<std::vector<std::shared_ptr<kdirent>>> kdirectory_impl::Entries() {
    directory *listing;
    if (file) {
        listing = dynamic_cast<directory *> (&(*file));
    } else {
        listing = nullptr;
    }
    std::shared_ptr<filesystem> fs{};
    {
        std::lock_guard lock{mounts_lock};
        for (auto mnt: mounts) {
            if (mnt.info.name == Name()) {
                fs = mnt.fs;
            }
        }
    }
    fsreference<directory> listingRef{};
    if (fs) {
        auto result = fs->GetRootDirectory(fs, selfRef.lock());
        bool failed{false};
        if (result.status != filesystem_status::SUCCESS) {
            failed = true;
            switch (result.status) {
                case filesystem_status::SUCCESS:
                    break;
                case filesystem_status::IO_ERROR:
                    std::cerr << "Mounted dir: " << name << ": Input/output error\n";
                    break;
                case filesystem_status::INTEGRITY_ERROR:
                    std::cerr << "Mounted dir: " << name << ": Integrity error\n";
                    break;
                case filesystem_status::NOT_SUPPORTED_FS_FEATURE:
                    std::cerr << "Mounted dir: " << name << ": Not supported filesystem feature\n";
                    break;
                case filesystem_status::INVALID_REQUEST:
                    std::cerr << "Mounted dir: " << name << ": Invalid request\n";
                    break;
                case filesystem_status::NO_AVAIL_INODES:
                    std::cerr << "Mounted dir: " << name << ": No available inodes\n";
                    break;
                case filesystem_status::NO_AVAIL_BLOCKS:
                    std::cerr << "Mounted dir: " << name << ": No available blocks\n";
                    break;
            }
        }
        if (!result.node) {
            failed = true;
            std::cerr << "Mounted dir: " << name << ": Rootdir not found\n";
        }
        if (!failed) {
            listingRef = std::move(result.node);
            listing = &(*listingRef);
        }
    }
    std::vector<std::shared_ptr<kdirent>> items{};
    std::shared_ptr<kdirectory_impl> this_ref_typed = selfRef.lock();
    items.push_back(std::make_shared<kdirent>(".", kdirectory_impl_ref::Create(this_ref_typed)));
    if (parent) {
        auto refCopy = parent.CreateReference(selfRef.lock());
        auto dirRef = reference_dynamic_cast<kdirectory_impl>(std::move(refCopy));
        items.push_back(std::make_shared<kdirent>("..", kdirectory_impl_ref::Create(dirRef)));
    } else {
        items.push_back(std::make_shared<kdirent>("..", kdirectory_impl_ref::Create(this_ref_typed)));
    }
    if (listing != nullptr) {
        auto entriesResult = listing->Entries();
        if (entriesResult.status != fileitem_status::SUCCESS) {
            return {.result = {}, .status = kfile_status::IO_ERROR};
        }
        for (auto fsent : entriesResult.entries) {
            auto name = fsent->Name();
            if (name != ".." && name != ".") {
                std::shared_ptr<lazy_kfile> fsfile = kdirectory_impl_lazy::Create(this_ref_typed, fsent, Name());
                items.push_back(std::make_shared<kdirent>(name, fsfile));
            }
        }
    }
    return {.result = items, .status = kfile_status::SUCCESS};
}

std::shared_ptr<filesystem> kdirectory_impl::Unmount() {
    {
        std::lock_guard lock{mounts_lock};
        auto iterator = mounts.end();
        while (iterator != mounts.begin()) {
            --iterator;
            auto &mnt = *iterator;
            if (mnt.info.name == Name()) {
                std::shared_ptr<filesystem> fs = mnt.fs;
                mounts.erase(iterator);
                return fs;
            }
        }
    }
    return {};
}
