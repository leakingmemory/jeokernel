include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(syscall OBJECT SyscallHandler.cpp SyscallHandler.h Exit.cpp Exit.h Write.cpp Write.h
        Geteuid.cpp Geteuid.h Getegid.cpp Getegid.h Getuid.cpp Getuid.h Getgid.cpp Getgid.h Brk.cpp Brk.h
        ArchPrctl.cpp ArchPrctl.h SetTidAddress.cpp SetTidAddress.h SetRobustList.cpp SetRobustList.h
        Uname.cpp Uname.h Writev.cpp Writev.h Mmap.cpp Mmap.h )

target_include_directories(syscall PRIVATE ../../include)
