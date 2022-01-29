include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(kshell OBJECT kshell.cpp kshell.h)

target_include_directories(kshell PRIVATE ../../include)