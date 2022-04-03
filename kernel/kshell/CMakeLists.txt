include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(kshell OBJECT kshell.cpp kshell.h kshell_commands.cpp kshell_commands.h kshell_ps.cpp kshell_ps.h kshell_stats.cpp kshell_stats.h kshell_blockdevices.cpp kshell_blockdevices.h )

target_include_directories(kshell PRIVATE ../../include)
target_include_directories(kshell PRIVATE ../blockdevs/include)
