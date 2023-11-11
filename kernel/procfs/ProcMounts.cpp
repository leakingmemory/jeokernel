//
// Created by sigsegv on 3/4/23.
//

#include "ProcMounts.h"
#include "procfs_fsresourcelockfactory.h"
#include <kfs/kfiles.h>
#include <sstream>

std::shared_ptr<ProcMounts> ProcMounts::Create() {
    procfs_fsresourcelockfactory lockfactory{};
    std::shared_ptr<ProcMounts> procMounts{new ProcMounts(lockfactory)};
    procMounts->SetSelfRef(procMounts);
    return procMounts;
}

std::string ProcMounts::GetContent() {
    std::stringstream strs{};
    {
        auto mounts = GetKmounts();
        for (const auto &mount: mounts) {
            strs << mount.devname << " " << mount.name << " " << mount.fstype << " " << mount.mntopts << " 0 0\n";
        }
    }
    return strs.str();
}