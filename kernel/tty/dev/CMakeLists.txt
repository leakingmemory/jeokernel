include(../../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(ttydev OBJECT ttydev.cpp ttydev.h ttyfsdev.cpp ttyfsdev.h)

target_include_directories(ttydev PRIVATE ../../../include)
target_include_directories(ttydev PRIVATE ../../blockdevs/include)

