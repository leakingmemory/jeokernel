include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(procfs OBJECT procfs.cpp)

target_include_directories(procfs PRIVATE ../../include)
target_include_directories(procfs PRIVATE ../blockdevs/include)
