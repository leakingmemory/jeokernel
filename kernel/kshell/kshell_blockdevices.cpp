//
// Created by sigsegv on 4/3/22.
//

#include "kshell_blockdevices.h"
#include <core/blockdevsystem.h>
#include <sstream>
#include <blockdevs/blockdev.h>

void kshell_blockdevices::Exec(kshell &, const std::vector<std::string> &cmd) {
    auto &system = get_blockdevsystem();
    auto names = system.GetBlockdevices();
    std::stringstream str{};
    for (const auto &name : names) {
        auto blockdev = system.GetBlockdevice(name);
        auto blocks = blockdev->GetNumBlocks();
        auto blocksize = blockdev->GetBlocksize();
        str << name << ": " << blocks << " with blocksize " << blocksize << "\n";
    }
    get_klogger() << str.str().c_str();
}