//
// Created by sigsegv on 8/14/22.
//

#include <iostream>
#include <exec/rseq.h>
#include <exec/usermem.h>
#include <core/scheduler.h>

#define RSEQ_DEBUG

int ThreadRSeq::Register(uintptr_t n_uptr_rseq, rseq &k_rseq, uint32_t n_sig) {
#ifdef RSEQ_DEBUG
    std::cout << "RSeq register " << std::hex << n_uptr_rseq << " sig " << n_sig << std::dec << "\n";
#endif
    uptr_rseq = n_uptr_rseq;
    sig = n_sig;
    return 0;
}

int ThreadRSeq::Unregister() {
#ifdef RSEQ_DEBUG
    std::cout << "RSeq unregister " << std::hex << uptr_rseq << " sig " << sig << std::dec << "\n";
#endif
    uptr_rseq = 0;
    sig = 0;
    return 0;
}

void ThreadRSeq::ResumeError() {
    std::cerr << "RSeq error on resume\n";
}

bool ThreadRSeq::Check(ProcThread &procThread, rseq &rs, Interrupt *intr) {
    if (intr == nullptr) {
        ResumeError();
        return false;
    }
    rsec_cs rcs{};
    UserMemory kmem_cs{procThread, (uintptr_t) rs.ptr, sizeof(rcs)};
    if (!kmem_cs) {
        ResumeError();
        return false;
    }
    memcpy(&rcs, kmem_cs.Pointer(), sizeof(rcs));
    auto rip = intr->get_cpu_frame().rip;
    if (rip < rcs.start_ip || (rip - rcs.start_ip) > rcs.post_commit_offset) {
        /* not within cs */
        rs.ptr = 0;
        return true;
    }
    auto flags = rs.flags | rcs.flags;
    auto preemptMask = (flags & RSEQ_CS_FLAG_NO_RESTART_ON_PREEMPT) != 0;
    auto signalMask = (flags & RSEQ_CS_FLAG_NO_RESTART_ON_SIGNAL) != 0;
    auto migrateMask = (flags & RSEQ_CS_FLAG_NO_RESTART_ON_MIGRATE) != 0;
    if (signalMask && (!migrateMask || !preemptMask)) {
        ResumeError();
        return false;
    }
    bool preempted;
    bool signaled;
    bool migrated;
    {
        critical_section cli{};
        preempted = this->preemted;
        signaled = this->signaled;
        migrated = this->migrated;
        this->preemted = false;
        this->signaled = false;
        this->migrated = false;
    }
    auto needRestart = (preempted && !preemptMask) || (signaled && !signalMask) || (migrated && !migrateMask);
    if (needRestart) {
        rs.ptr = 0;
        auto &frame = intr->get_cpu_frame();
#ifdef RSEQ_DEBUG
        std::cout << "RSeq restart (/abort): "<< std::hex << rcs.abort_ip << " at " << frame.rip << std::dec << "\n";
#endif
        frame.rip = rcs.abort_ip;
    }
    return true;
}

void ThreadRSeq::Notify(ProcThread &procThread, task &t, Interrupt *intr, uint8_t cpu) {
    if (cpu != this->cpuid) {
        this->migrated = true;
    }
    if (uptr_rseq == 0) {
        this->cpuid = cpu;
        return;
    }
    rseq *rs;
    UserMemory kmem{procThread, (uintptr_t) uptr_rseq, sizeof(*rs), true};
    if (!kmem) {
        ResumeError();
        this->cpuid = cpu;
        return;
    }
    rs = (rseq *) kmem.Pointer();
    if (rs->ptr == 0) {
        this->cpuid = cpu;
        return;
    }
#ifdef RSEQ_DEBUG
    std::cout << "Active RSeq check: " << std::hex << rs->ptr << std::dec << "\n";
#endif
    if (Check(procThread, *rs, intr)) {
        rs->cpu_id = cpu;
        rs->cpu_id_start = cpu;
    }
    this->cpuid = cpu;
}
