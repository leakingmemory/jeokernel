//
// Created by sigsegv on 1/30/22.
//

#include "kshell_ps.h"
#include "sstream"

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

void kshell_ps::Exec(kshell &, const std::vector<std::string> &cmd) {
    auto tasks = get_scheduler()->get_task_infos();
    get_klogger() << "PID     CPU   FLA PRI PTS NAME\n";
    for (auto &task : tasks) {
        std::stringstream str{};
        str << task.bits.id;
        tab(str, 8);
        str << "cpu" << task.bits.cpu;
        tab(str, 2);
        str << (task.bits.running ? "R" : (task.bits.blocked ? "B" : "r"));
        str << (task.bits.cpu_pinned ? "p" : "");
        str << (task.bits.stack_quarantine ? "q" : "");
        tab(str, 9);
        str << task.bits.priority_group;
        tab(str, 4);
        str << task.bits.points;
        tab(str,4);
        str << task.name;
        str << "\n";
        get_klogger() << str.str().c_str();
    }
}
