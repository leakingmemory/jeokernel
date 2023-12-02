include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(prockshell OBJECT prockshell.cpp kshell_ups.cpp kshell_ups.h)

target_include_directories(prockshell PRIVATE ../../include)
