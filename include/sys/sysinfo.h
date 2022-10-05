//
// Created by sigsegv on 10/5/22.
//

#ifndef JEOKERNEL_SYSINFO_H
#define JEOKERNEL_SYSINFO_H

struct sysinfo {
    long uptime;
    unsigned long loads[3];
    unsigned long totalram;
    unsigned long freeram;
    unsigned long sharedram;
    unsigned long bufferram;
    unsigned long totalswap;
    unsigned long freeswap;
    unsigned short procs;
    unsigned long totalhigh;
    unsigned long freehigh;
    unsigned int mem_unit;
};

void physmem_stats(sysinfo &);

#endif //JEOKERNEL_SYSINFO_H
