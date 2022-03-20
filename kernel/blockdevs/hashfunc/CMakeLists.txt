cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)

add_library(hashfunc OBJECT crc32.cpp hashfunc.cpp)

target_include_directories(hashfunc PRIVATE ../include)
