//
// Created by sigsegv on 6/6/22.
//

#include <core/blockdevsystem.h>
#include <filesystems/filesystem.h>
#include <kfs/blockdev_writer.h>
#include <resource/referrer.h>
#include "kshell_mount.h"
#include "kshell_argsparser.h"
#include "iostream"

class kshell_mount_exec : public referrer {
private:
    std::weak_ptr<kshell_mount_exec> selfRef{};
    kshell_mount_exec() : referrer("kshell_mount_exec") {}
public:
    static std::shared_ptr<kshell_mount_exec> Create();
    kshell_mount_exec(const kshell_mount_exec &) = delete;
    kshell_mount_exec(kshell_mount_exec &&) = delete;
    kshell_mount_exec &operator =(const kshell_mount_exec &) = delete;
    kshell_mount_exec &operator =(kshell_mount_exec &&) = delete;
    std::string GetReferrerIdentifier() override;
    void Exec(kshell &shell, const std::string &fstype, const std::string &deviceName, const std::string &mountpoint_orig);
};

std::shared_ptr<kshell_mount_exec> kshell_mount_exec::Create() {
    std::shared_ptr<kshell_mount_exec> e{new kshell_mount_exec()};
    std::weak_ptr<kshell_mount_exec> w{e};
    e->selfRef = w;
    return e;
}

std::string kshell_mount_exec::GetReferrerIdentifier() {
    return "";
}

void kshell_mount_exec::Exec(kshell &shell, const std::string &fstype, const std::string &deviceName, const std::string &mountpoint_orig) {
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
        mountfile = shell.CwdRef(selfRef);
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

    auto &system = get_blockdevsystem();
    auto blockdev = system.GetBlockdevice(deviceName);

    std::shared_ptr<filesystem> fs{};

    if (!blockdev) {
        fs = system.OpenFilesystem(fstype);
        if (!fs) {
            std::cerr << "Device not found " << deviceName << "\n";
            return;
        }
    }

    if (!fs) {
        fs = system.OpenFilesystem(fstype, blockdev);
    }
    if (fs) {
        blockdev_writer::GetInstance().OpenForWrite(fs);
    } else {
        std::cerr << "Failed to mount filesystem " << fstype << " on device " << deviceName << "\n";
        return;
    }

    std::string name{"/dev/"};
    name.append(deviceName);
    dir->Mount(name, fstype, "ro", fs);
}

void kshell_mount::Exec(kshell &shell, const std::vector<std::string> &cmd) {
    kshell_argsparser parser{[] (char opt) {
        switch (opt) {
            case 't':
                return 1;
            default:
                return -1;
        }
    }};
    std::string fstype{};
    std::string deviceName{};
    std::string mountpoint{};
    auto iterator = cmd.begin();
    if (iterator != cmd.end()) {
        ++iterator;
    }
    bool parseSuccessful = parser.Parse(iterator, cmd.end(), [&fstype] (char opt, const std::vector<std::string> &params) {
        switch (opt) {
            case 't':
                fstype = params[0];
                break;
        }
    });
    if (!parseSuccessful) {
        return;
    }
    if (iterator == cmd.end()) {
        std::cerr << "Expected device to mount\n";
        return;
    }
    deviceName = *iterator;
    ++iterator;
    if (iterator == cmd.end()) {
        std::cerr << "Expected mountpoint\n";
        return;
    }
    mountpoint = *iterator;
    ++iterator;

    if (iterator != cmd.end()) {
        std::cerr << "Trailing arguments, too many parameters?\n";
        return;
    }

    kshell_mount_exec::Create()->Exec(shell, fstype, deviceName, mountpoint);
}