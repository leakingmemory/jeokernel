//
// Created by sigsegv on 3/3/23.
//

#include "ProcMeminfo.h"
#include "procfs_fsresourcelockfactory.h"
#include <sstream>
#include <sys/sysinfo.h>

std::shared_ptr<ProcMeminfo> ProcMeminfo::Create() {
    procfs_fsresourcelockfactory lockfactory{};
    std::shared_ptr<ProcMeminfo> procMeminfo{new ProcMeminfo(lockfactory)};
    procMeminfo->SetSelfRef(procMeminfo);
    return procMeminfo;
}

static void Line(std::stringstream &strs, const char *name, uint64_t value)
{
    std::string strValue{};
    {
        std::stringstream svalue{};
        svalue << std::dec << value;
        strValue = svalue.str();
    }
    size_t len = strlen(name) + strValue.size() + 2;
    strs << name << ": ";
    if (len < 32) {
        auto padding = 32 - len;
        std::string padStr{};
        padStr.append("                              ", padding);
        strs << padStr;
    }
    strs << strValue;
}

static void Bytes(std::stringstream &strs, const char *name, uint64_t value)
{
    Line(strs, name, (value >> 10));
    strs << " kB\n";
}

static void Count(std::stringstream &strs, const char *name, uint64_t value)
{
    Line(strs, name, (value >> 10));
    strs << "\n";
}

std::string ProcMeminfo::GetContent() {
    std::stringstream strs{};
    {
        sysinfo sinfo{};
        physmem_stats(sinfo);
        uint64_t memTotal{sinfo.totalram};
        memTotal *= sinfo.mem_unit;
        uint64_t memFree{sinfo.freeram};
        memFree *= sinfo.mem_unit;
        Bytes(strs, "MemTotal", memTotal);
        Bytes(strs, "MemFree", memFree);
        Bytes(strs, "MemAvailable", memFree);
        Bytes(strs, "Buffers", 0);
        Bytes(strs, "Cached", 0);
        Bytes(strs, "SwapCached", 0);
        uint64_t Active{memTotal - memFree};
        Bytes(strs, "Active", Active);
        Bytes(strs, "Inactive", 0);
        Bytes(strs, "Active(anon)", 0);
        Bytes(strs, "Inactive(anon)", 0);
        Bytes(strs, "Active(file)", 0);
        Bytes(strs, "Inactive(file)", 0);
        Bytes(strs, "Unevictable", 0);
        Bytes(strs, "Mlocked", 0);
        Bytes(strs, "SwapTotal", 0);
        Bytes(strs, "SwapFree", 0);
        Bytes(strs, "Zswap", 0);
        Bytes(strs, "Zswapped", 0);
        Bytes(strs, "Dirty", 0);
        Bytes(strs, "Writeback", 0);
        Bytes(strs, "AnonPages", 0);
        Bytes(strs, "Mapped", 0);
        Bytes(strs, "Shmem", 0);
        Bytes(strs, "KReclaimable", 0);
        Bytes(strs, "Slab", 0);
        Bytes(strs, "SReclaimable", 0);
        Bytes(strs, "SUnreclaim", 0);
        Bytes(strs, "KernelStack", 0);
        Bytes(strs, "PageTables", 0);
        Bytes(strs, "SecPageTables", 0);
        Bytes(strs, "NFS_Unstable", 0);
        Bytes(strs, "Bounce", 0);
        Bytes(strs, "WritebackTmp", 0);
        Bytes(strs, "CommitLimit", 0);
        Bytes(strs, "Committed_AS", 0);
        Bytes(strs, "VmallocTotal", 0);
        Bytes(strs, "VmallocUsed", 0);
        Bytes(strs, "VmallocChunk", 0);
        Bytes(strs, "Percpu", 0);
        Bytes(strs, "HardwareCorrupted", 0);
        Bytes(strs, "AnonHugePages", 0);
        Bytes(strs, "ShmemHugePages", 0);
        Bytes(strs, "ShmemPmdMapped", 0);
        Bytes(strs, "FileHugePages", 0);
        Bytes(strs, "FilePmdMapped", 0);
        Bytes(strs, "CmaTotal", 0);
        Bytes(strs, "CmaFree", 0);
        Count(strs, "HugePages_Total", 0);
        Count(strs, "HugePages_Free", 0);
        Count(strs, "HugePages_Rsvd", 0);
        Count(strs, "HugePages_Surp", 0);
        Bytes(strs, "Hugepagesize", 0);
        Bytes(strs, "Hugetlb", 0);
        Bytes(strs, "DirectMap4k", 0);
        Bytes(strs, "DirectMap2M", 0);
        Bytes(strs, "DirectMap1G", 0);
    }
    return strs.str();
}