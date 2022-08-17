//
// Created by sigsegv on 8/12/22.
//

#ifndef JEOKERNEL_RSEQ_H
#define JEOKERNEL_RSEQ_H

#include <cstdint>

struct rseq {
    uint32_t cpu_id_start;
    uint32_t cpu_id;
    uint64_t ptr;
    uint32_t flags;
} __attribute__((aligned(4*8)));

struct rsec_cs {
    uint32_t version;
    uint32_t flags;
    uint64_t start_ip;
    uint64_t post_commit_offset;
    uint64_t abort_ip;
} __attribute__((aligned(4*8)));

class ProcThread;
class task;
class Interrupt;

class ThreadRSeq {
private:
    uintptr_t uptr_rseq;
    uint32_t sig;
    uint8_t cpuid;
    bool preemted : 1;
    bool signaled : 1;
    bool migrated : 1;
public:
    ThreadRSeq() : uptr_rseq(0), sig(0), cpuid(0), preemted(false), signaled(false), migrated(false) {}
    uintptr_t U_Rseq() {
        return uptr_rseq;
    }
    uint32_t Sig() {
        return sig;
    }
    bool IsRegistered() {
        return uptr_rseq != 0;
    }
    int Unregister();
    int Register(uintptr_t uptr_rseq, rseq &k_rseq, uint32_t sig);
    void ResumeError();
private:
    bool Check(ProcThread &, rseq &, Interrupt *intr);
public:
    void Notify(ProcThread &, task &, Interrupt *intr, uint8_t cpu);
    void Preempted() {
        preemted = true;
    }
};

#define RSEQ_FLAG_UNREGISTER    (1 << 0)

#define RSEQ_CS_FLAG_NO_RESTART_ON_PREEMPT  (1 << 0)
#define RSEQ_CS_FLAG_NO_RESTART_ON_SIGNAL   (1 << 1)
#define RSEQ_CS_FLAG_NO_RESTART_ON_MIGRATE  (1 << 2)

#endif //JEOKERNEL_RSEQ_H
