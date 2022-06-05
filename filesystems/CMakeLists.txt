cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)

add_library(filesystems OBJECT ext2fs.cpp ext2fs.h filesystem_impls.cpp filesystems.cpp)

target_include_directories(filesystems PRIVATE ../include)

target_include_directories(filesystems PRIVATE ../../../include)
