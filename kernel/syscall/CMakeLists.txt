include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(impl)

add_library(syscall OBJECT SyscallHandler.cpp SyscallHandler.h Exit.cpp Exit.h Write.cpp Write.h
        Geteuid.cpp Geteuid.h Getegid.cpp Getegid.h Getuid.cpp Getuid.h Getgid.cpp Getgid.h Brk.cpp Brk.h
        ArchPrctl.cpp ArchPrctl.h SetTidAddress.cpp SetTidAddress.h SetRobustList.cpp SetRobustList.h
        Uname.cpp Uname.h Writev.cpp Writev.h Mmap.cpp Mmap.h Sigprocmask.cpp Sigprocmask.h
        PrLimit64.cpp PrLimit64.h Readlink.cpp Readlink.h Getrandom.cpp Getrandom.h
        ClockGettime.cpp ClockGettime.h Mprotect.cpp Mprotect.h Newfstatat.cpp Newfstatat.h
        ExitGroup.cpp ExitGroup.h Rseq.cpp Rseq.h Futex.cpp Futex.h Access.cpp Access.h
        SyscallAsyncThread.cpp SyscallAsyncThread.h OpenAt.cpp OpenAt.h Read.cpp Read.h Close.cpp Close.h
        Pread64.cpp Pread64.h Gettimeofday.cpp Gettimeofday.h Ioctl.cpp Ioctl.h SyscallCtx.cpp SyscallCtx.h
        Sysinfo.cpp Sysinfo.h RtSigaction.cpp RtSigaction.h Getcwd.cpp Getcwd.h Getpid.cpp Getpid.h
        Getppid.cpp Getppid.h Getpgrp.cpp Getpgrp.h Dup.cpp Dup.h Fcntl.cpp Fcntl.h Dup2.cpp Dup2.h Time.cpp Time.h
        Pselect6.cpp Pselect6.h Setpgid.cpp Setpgid.h Clone.cpp Clone.h Wait4.cpp Wait4.h Execve.cpp Execve.h
        Prctl.cpp Prctl.h Getdents64.cpp Getdents64.h Statx.cpp Statx.h Chdir.cpp Chdir.h Munmap.cpp Munmap.h
        Getxattr.cpp Getxattr.h SchedGetaffinity.cpp SchedGetaffinity.h Fstat.cpp Fstat.h Fadvise64.cpp Fadvise64.h
        Lseek.cpp Lseek.h Mkdir.cpp Mkdir.h Mount.cpp Mount.h Open.cpp Open.h Getpgid.cpp Getpgid.h Stat.cpp Stat.h
        Fork.cpp Fork.h Gettid.cpp Gettid.h Readv.cpp Readv.h Faccessat2.cpp Faccessat2.h Membarrier.cpp Membarrier.h
        Nanosleep.cpp Nanosleep.h Sigaltstack.cpp Sigaltstack.h)

target_include_directories(syscall PRIVATE ../../include)
