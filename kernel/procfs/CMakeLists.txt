include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(procfs OBJECT procfs.cpp ProcDatafile.cpp ProcDatafile.h ProcAuxv.cpp ProcAuxv.h
        ProcStrfile.cpp ProcStrfile.h ProcLazyStrfile.cpp ProcLazyStrfile.h ProcProcessStat.cpp ProcProcessStat.h)

target_include_directories(procfs PRIVATE ../../include)
target_include_directories(procfs PRIVATE ../blockdevs/include)
