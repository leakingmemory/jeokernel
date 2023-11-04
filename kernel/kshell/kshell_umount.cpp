//
// Created by sigsegv on 11/3/23.
//

#include "kshell_umount.h"
#include <kfs/blockdev_writer.h>
#include <iostream>

void kshell_umount::Exec(kshell &sh, const std::vector<std::string> &cmd) {
    auto iterator = cmd.begin();
    if (iterator != cmd.end()) {
        ++iterator;
    }
    if (iterator == cmd.end()) {
        std::cerr << "Expected device to mount\n";
        return;
    }

    std::string mountpoint = *iterator;
    ++iterator;

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
        mountfile = sh.CwdRef();
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

    auto fs = dir->Unmount();

    if (!fs) {
        std::cerr << mountpoint_orig << ": Not mounted\n";
    }

    blockdev_writer::GetInstance().CloseForWrite(fs);
}