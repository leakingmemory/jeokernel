include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(kfs OBJECT kfiles.cpp)

target_include_directories(kfs PRIVATE ../../include)
target_include_directories(kfs PRIVATE ../blockdevs/include)
