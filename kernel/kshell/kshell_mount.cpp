//
// Created by sigsegv on 6/6/22.
//

#include <core/blockdevsystem.h>
#include <filesystems/filesystem.h>
#include <kfs/blockdev_writer.h>
#include "kshell_mount.h"
#include "kshell_argsparser.h"
#include "iostream"

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

    std::string mountpoint_orig{mountpoint};
    std::shared_ptr<kfile> mountfile{};
    auto rootdir = get_kernel_rootdir();
    if (mountpoint.starts_with("/")) {
        while (mountpoint.starts_with("/")) {
            std::string trim{};
            trim.append(mountpoint.c_str() + 1);
            mountpoint = trim;
        }
        mountfile = rootdir;
    } else {
        mountfile = shell.CwdRef();
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
        auto resolveResult = dir->Resolve(&(*rootdir), mountpoint);
        if (resolveResult.status != kfile_status::SUCCESS) {
            std::cerr << "Error: " << text(resolveResult.status) << "\n";
            return;
        }
        mountfile = resolveResult.result;
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
        fs = open_filesystem(fstype);
        if (!fs) {
            std::cerr << "Device not found " << deviceName << "\n";
            return;
        }
    }

    if (!fs) {
        fs = open_filesystem(fstype, blockdev);
    }
    if (fs) {
        blockdev_writer::GetInstance().OpenForWrite(fs);
    } else {
        std::cerr << "Failed to mount filesystem " << fstype << " on device " << deviceName << "\n";
        return;
    }

    auto result = fs->GetRootDirectory(fs);
    if (result.node) {
        std::string name{"/dev/"};
        name.append(deviceName);
        dir->Mount(name, fstype, "ro", result.node);
    } else {
        std::cerr << "Failed to mount filesystem " << fstype << " on device " << deviceName << ": "
                  << text(result.status) << "\n";
    }
}