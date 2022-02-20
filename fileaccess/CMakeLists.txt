cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)

add_library(fileaccess OBJECT file_blockdev.cpp)

target_include_directories(fileaccess PRIVATE ../include)
