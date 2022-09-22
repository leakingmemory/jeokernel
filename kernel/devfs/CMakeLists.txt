include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(devfs OBJECT devfs.cpp provider.cpp)

target_include_directories(devfs PRIVATE ../../include)
target_include_directories(devfs PRIVATE ../blockdevs/include)
