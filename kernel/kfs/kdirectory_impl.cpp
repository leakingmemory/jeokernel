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
static std::shared_ptr<kdirectory_impl> rootdir = kdirectory_impl::Create({}, {}, "/");

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
    return std::make_shared<kdirectory>(rootdir, "/");
}

void kdirectory_impl::Mount(const std::string &devname, const std::string &fstype, const std::string &mntopts, const std::shared_ptr<filesystem> &fs) {
    std::lock_guard lock{mounts_lock};
    mounts.push_back({.info = {.devname = devname, .fstype = fstype, .mntopts = mntopts, .name = Name()}, .fs = fs});
}

std::string kdirectory_impl::Kpath() {
    return Name();
}

void kdirectory_impl::Init(std::shared_ptr<kdirectory_impl> &selfRef) {
    std::weak_ptr<kdirectory_impl> weak{selfRef};
    this->selfRef = weak;
}

std::shared_ptr<kdirectory_impl>
kdirectory_impl::Create(const fsreference<fileitem> &file, const std::shared_ptr<kfile> &parent, const std::string &kpath) {
    auto dir = kfile_impl::Create<kdirectory_impl>(file, parent, kpath);
    dir->Init(dir);
    return dir;
}

kfile_result<std::vector<std::shared_ptr<kdirent>>> kdirectory_impl::Entries(std::shared_ptr<kfile> this_ref) {
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
    items.push_back(std::make_shared<kdirent>(".", std::make_shared<kdirectory_impl_ref>(this_ref)));
    if (parent) {
        items.push_back(std::make_shared<kdirent>("..", std::make_shared<kdirectory_impl_ref>(parent)));
    } else {
        items.push_back(std::make_shared<kdirent>("..", std::make_shared<kdirectory_impl_ref>(this_ref)));
    }
    if (listing != nullptr) {
        auto entriesResult = listing->Entries();
        if (entriesResult.status != fileitem_status::SUCCESS) {
            return {.result = {}, .status = kfile_status::IO_ERROR};
        }
        for (auto fsent : entriesResult.entries) {
            auto name = fsent->Name();
            if (name != ".." && name != ".") {
                std::shared_ptr<lazy_kfile> fsfile = kdirectory_impl_lazy::Create(this_ref, fsent, Name());
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
