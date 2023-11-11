//
// Created by sigsegv on 2/28/23.
//

#include "ProcUptime.h"
#include "procfs_fsresourcelockfactory.h"
#include <core/nanotime.h>
#include <sstream>

std::shared_ptr<ProcUptime> ProcUptime::Create() {
    procfs_fsresourcelockfactory lockfactory{};
    std::shared_ptr<ProcUptime> procUptime{new ProcUptime(lockfactory)};
    procUptime->SetSelfRef(procUptime);
    return procUptime;
}

std::string ProcUptime::GetContent() {
    auto tm = get_nanotime_ref();
    auto csec = tm / 10000000;
    auto sec = csec / 100;
    csec = csec % 100;
    std::stringstream sstr{};
    sstr << sec << ".";
    if (csec < 10) {
        sstr << "0";
    }
    sstr << csec << " 0.00\n";
    return sstr.str();
}