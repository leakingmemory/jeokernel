//
// Created by sigsegv on 11/3/23.
//

#include "kshell_umount.h"
#include <kfs/blockdev_writer.h>
#include <resource/referrer.h>
#include <resource/reference.h>
#include <iostream>

class kshell_umount_exec : public referrer {
private:
    std::weak_ptr<kshell_umount_exec> selfRef{};
    kshell_umount_exec() : referrer("kshell_umount_exec") {}
public:
    kshell_umount_exec(const kshell_umount_exec &) = delete;
    kshell_umount_exec(kshell_umount_exec &&) = delete;
    kshell_umount_exec &operator =(const kshell_umount_exec &) = delete;
    kshell_umount_exec &operator =(kshell_umount_exec &&) = delete;
    static std::shared_ptr<kshell_umount_exec> Create();
    std::string GetReferrerIdentifier() override;
    void Exec(kshell &sh, const std::string &mountpoint_orig);
};

std::shared_ptr<kshell_umount_exec> kshell_umount_exec::Create() {
    std::shared_ptr<kshell_umount_exec> e{new kshell_umount_exec()};
    std::weak_ptr<kshell_umount_exec> w{e};
    e->selfRef = w;
    return e;
}

std::string kshell_umount_exec::GetReferrerIdentifier() {
    return "";
}

void kshell_umount_exec::Exec(kshell &sh, const std::string &mountpoint_orig) {
    std::string mountpoint{mountpoint_orig};
    reference<kfile> mountfile{};
    auto rootdir = get_kernel_rootdir();
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    if (mountpoint.starts_with("/")) {
        while (mountpoint.starts_with("/")) {
            std::string trim{};
            trim.append(mountpoint.c_str() + 1);
            mountpoint = trim;
        }
        mountfile = rootdir->CreateReference(selfRef);
    } else {
        mountfile = sh.CwdRef(selfRef);
        if (mountpoint.empty()) {
            std::cerr << "Mountpoint not found " << mountpoint_orig << "\n";
            return;
        }
    }
    if (!mountpoint.empty()) {
        kdirectory *dir = dynamic_cast<kdirectory *> (&(*mountfile));
        if (dir == nullptr) {
            std::cerr << "Mountpoint not found " << mountpoint_orig << "\n";
            return;
        }
        auto resolveResult = dir->Resolve(&(*rootdir), selfRef, mountpoint);
        if (resolveResult.status != kfile_status::SUCCESS) {
            std::cerr << "Error: " << text(resolveResult.status) << "\n";
            return;
        }
        mountfile = std::move(resolveResult.result);
        if (!mountfile) {
            std::cerr << "Mountpoint not found " << mountpoint_orig << "\n";
            return;
        }
    }

    kdirectory *dir = dynamic_cast<kdirectory *> (&(*mountfile));
    if (dir == nullptr) {
        std::cerr << "Mountpoint not found " << mountpoint_orig << ": Not a directory\n";
        return;
    }

    auto fs = dir->Unmount();

    if (!fs) {
        std::cerr << mountpoint_orig << ": Not mounted\n";
    }

    blockdev_writer::GetInstance().CloseForWrite(fs);
}

void kshell_umount::Exec(kshell &sh, const std::vector<std::string> &cmd) {
    auto iterator = cmd.begin();
    if (iterator != cmd.end()) {
        ++iterator;
    }
    if (iterator == cmd.end()) {
        std::cerr << "Expected device to unmount\n";
        return;
    }

    std::string mountpoint = *iterator;
    ++iterator;

    if (iterator != cmd.end()) {
        std::cerr << "Unexpected arguments to unmount\n";
        return;
    }

    kshell_umount_exec::Create()->Exec(sh, mountpoint);
}