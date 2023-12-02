//
// Created by sigsegv on 12/2/23.
//

#include "kshell_ups.h"
#include <exec/procthread.h>
#include <sstream>

static void tab(std::stringstream &str, uint8_t characters) {
    auto length = str.str().size();
    auto pad = characters - (length % characters);
    if (pad == 0) {
        pad = characters;
    }
    std::string padding{"                "};
    padding.resize(pad);
    str << padding;
}

struct ProcData {
    std::shared_ptr<Process> process;
    std::string name;
    int syscallNumber;
    uint32_t taskId;
};

void kshell_ups::Exec(kshell &shell, const std::vector<std::string> &cmd) {
    auto *scheduler = get_scheduler();
    std::vector<ProcData> procs{};
    {
        int numProcs{0};
        scheduler->all_tasks([&numProcs](task &) {
            ++numProcs;
        });
        numProcs += 10;
        procs.reserve(numProcs);
    }
    scheduler->all_tasks([&procs] (task &t) {
        auto *thr = t.get_resource<ProcThread>();
        if (thr != nullptr) {
            ProcData data{
                    .process = thr->GetProcess(),
                    .name = t.get_name(),
                    .syscallNumber = thr->GetSyscallNumber(),
                    .taskId = t.get_id()
            };
            procs.emplace_back(std::move(data));
        }
    });
    if (procs.empty()) {
        return;
    }
    std::cout << "PID     KID     Syscall Name\n";
    for (const auto &proc : procs) {
        std::stringstream sstr{};
        sstr << proc.process->getpid();
        tab(sstr, 8);
        sstr << proc.taskId;
        tab(sstr, 16);
        sstr << proc.syscallNumber;
        tab(sstr, 24);
        sstr << proc.name << "\n";
        std::cout << sstr.str();
    }
}