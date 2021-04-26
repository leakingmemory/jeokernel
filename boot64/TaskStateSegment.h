//
// Created by sigsegv on 25.04.2021.
//

#ifndef JEOKERNEL_TASKSTATESEGMENT_H
#define JEOKERNEL_TASKSTATESEGMENT_H

#include "GlobalDescriptorTable.h"
#include <core/malloc.h>
#include <klogger.h>
#include <string.h>

struct TSS_entry {
    uint32_t reserved1; // 00
    uint64_t rsp0; // 04
    uint64_t rsp1; // 0C
    uint64_t rsp2; // 14
    uint64_t reserved2; // 1C
    uint64_t ist1; // 24
    uint64_t ist2; // 2C
    uint64_t ist3; // 34
    uint64_t ist4; // 3C
    uint64_t ist5; // 44
    uint64_t ist6; // 4C
    uint64_t ist7; // 54
    uint64_t reserved3; // 5C
    uint16_t reserved4; // 64
    uint16_t iopb_offset; // 66
    // 68
    uint64_t cetssp;

    TSS_entry() {
        memset((void *) this, 0, sizeof(*this));
        iopb_offset = sizeof(*this);
    }
} __attribute__((__packed__));
static_assert(sizeof(TSS_entry) == 0x70);

struct IOPB {
    uint8_t iopb[8192];

    IOPB() {
        memset((void *) &(iopb[0]), 0, sizeof(iopb));
        iopb[8191] = 0xFF;
    }
} __attribute__((__packed__));

struct TSS_entry_with_iobmap {
    TSS_entry tss;
    IOPB iopb;

    TSS_entry_with_iobmap() : tss(), iopb() {
    }
} __attribute__((__packed__));

class TaskStateSegment {
private:
    TSS_entry_with_iobmap *tss_entry;
public:
    TaskStateSegment();
    /*
     * Note that destroying a TSS in use is obviously UB
     */
    ~TaskStateSegment();

    TSS_entry &get_entry() {
        return (*tss_entry).tss;
    }

    /*IOPB &get_iopb() {
        return (*tss_entry).iopb;
    }*/

    /**
     *
     * @param gdt
     * @param cpu_n
     * @return TSS selector
     */
    int install(GlobalDescriptorTable &gdt, int cpu_n);
    int install_cpu(GlobalDescriptorTable &gdt, int cpu_n);

    /*
     * Constructed in place and stays in place. No copies, moves.
     */
    TaskStateSegment(const TaskStateSegment &) = delete;
    TaskStateSegment(TaskStateSegment &&) = delete;
    TaskStateSegment & operator = (const TaskStateSegment &) = delete;
    TaskStateSegment & operator = (TaskStateSegment &&) = delete;
};


#endif //JEOKERNEL_TASKSTATESEGMENT_H
