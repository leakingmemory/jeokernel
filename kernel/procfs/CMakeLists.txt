include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(procfs OBJECT procfs.cpp ProcDatafile.cpp ProcDatafile.h ProcAuxv.cpp ProcAuxv.h
        ProcStrfile.cpp ProcStrfile.h ProcLazyStrfile.cpp ProcLazyStrfile.h ProcProcessStat.cpp ProcProcessStat.h
        ProcUptime.cpp ProcUptime.h ProcSysLazyStrfile.cpp ProcSysLazyStrfile.h ProcMeminfo.cpp ProcMeminfo.h
        ProcMounts.cpp ProcMounts.h procfs_fsresourcelockfactory.cpp procfs_fsresourcelockfactory.h)

target_include_directories(procfs PRIVATE ../../include)
target_include_directories(procfs PRIVATE ../blockdevs/include)
