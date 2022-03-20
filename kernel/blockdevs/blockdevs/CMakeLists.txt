cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)

add_library(blockdevs OBJECT mbr_reader.cpp parttable_readers.cpp offset_blockdev.cpp gpt_reader.cpp)

target_include_directories(blockdevs PRIVATE ../include)

target_include_directories(blockdevs PRIVATE ../../../include)

