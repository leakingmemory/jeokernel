include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(kshell OBJECT kshell.cpp kshell_commands.cpp kshell_ps.cpp kshell_ps.h kshell_stats.cpp kshell_stats.h kshell_blockdevices.cpp kshell_blockdevices.h kshell_ls.cpp kshell_ls.h kshell_mount.cpp kshell_mount.h kshell_argsparser.cpp kshell_argsparser.h kshell_cd.cpp kshell_cd.h kshell_cat.cpp kshell_cat.h kshell_exec.cpp kshell_exec.h kshell_umount.cpp kshell_umount.h )

target_include_directories(kshell PRIVATE ../../include)
target_include_directories(kshell PRIVATE ../blockdevs/include)
