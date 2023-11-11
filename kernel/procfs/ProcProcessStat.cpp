//
// Created by sigsegv on 2/27/23.
//

#include "ProcProcessStat.h"
#include "procfs_fsresourcelockfactory.h"
#include <exec/procthread.h>
#include <sstream>

std::shared_ptr<ProcProcessStat> ProcProcessStat::Create(const std::shared_ptr<Process> &process) {
    procfs_fsresourcelockfactory lockfactory{};
    std::shared_ptr<ProcProcessStat> procProcessStat{new ProcProcessStat(lockfactory, process)};
    procProcessStat->SetSelfRef(procProcessStat);
    return procProcessStat;
}

std::string ProcProcessStat::GetContent(Process &proc) {
    auto pid = proc.getpid();
    auto ppid = proc.getppid();
    auto pgrp = proc.getpgrp();
    auto session = 0;
    auto tty_nr = 0;
    auto tpgid = -1;
    auto flags = 0;
    auto minflt = 0;
    auto cminflt = 0;
    auto majflt = 0;
    auto cmajflt = 0;
    auto utime = 0;
    auto stime = 0;
    auto cutime = 0;
    auto cstime = 0;
    auto priority = 0;
    auto nice = 0;
    auto num_threads = 0;
    auto itrealvalue = 0;
    auto starttime = 0;
    auto vsize = 0;
    auto rss = 0;
    auto rsslim = 0;
    auto startcode = 0;
    auto endcode = 0;
    auto startstack = 0;
    auto kstkesp = 0;
    auto kstkeip = 0;
    auto signal = 0;
    auto blocked = 0;
    auto sigignore = 0;
    auto sigcatch = 0;
    auto wchan = 0;
    auto nswap = 0;
    auto cnswap = 0;
    auto exit_signal = 0;
    auto processor = 0;
    auto rt_priority = 0;
    auto policy = 0;
    auto delayacct_blkio_ticks = 0;
    auto guest_time = 0;
    auto cguest_time = 0;
    auto start_data = 0;
    auto end_data = 0;
    auto start_brk = 0;
    auto arg_start = 0;
    auto arg_end = 0;
    auto env_start = 0;
    auto env_end = 0;
    auto exit_code = 0;
    auto name = proc.GetCmdline();
    std::string state{"Z"};
    get_scheduler()->all_tasks([pid, &state, &priority, &num_threads, &processor] (task &t) {
        auto thread = t.get_resource<ProcThread>();
        if (thread != nullptr && thread->getpid() == pid) {
            ++num_threads;
            if (t.is_blocked()) {
                if (state == "Z" || state == "X") {
                    state = "S";
                    priority = t.get_points();
                    processor = t.get_cpu();
                }
            } else if (t.is_end()) {
                if (state == "Z") {
                    state = "X";
                    priority = t.get_points();
                    processor = t.get_cpu();
                }
            } else {
                state = "R";
                priority = t.get_points();
                processor = t.get_cpu();
            }
        }
    });
    std::stringstream strs{};
    strs << std::dec << pid << " (" << name << ") " << state << " " << ppid << " " << pgrp << " " << session << " "
         << tty_nr << " " << tpgid << " " << flags << " " << minflt << " " << cminflt << " " << majflt << " "
         << cmajflt << " " << utime << " " << stime << " " << cutime << " " << cstime << " " << priority << " " << nice
         << " " << num_threads << " " << itrealvalue << " " << starttime << " " << vsize << " " << rss << " " << rsslim
         << " " << startcode << " " << endcode << " " << startstack << " " << kstkesp << " " << kstkeip << " " << signal
         << " " << blocked << " " << sigignore << " " << sigcatch << " " << rt_priority << " " << policy << " "
         << delayacct_blkio_ticks << " " << guest_time << " " << cguest_time << " " << start_data << " " << end_data
         << " " << start_brk << " " << arg_start << " " << arg_end << " " << env_start << " " << env_end << " "
         << exit_code << "\n";
    return strs.str();
}