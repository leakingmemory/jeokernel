include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(syscall OBJECT SyscallHandler.cpp SyscallHandler.h)

target_include_directories(syscall PRIVATE ../../include)
