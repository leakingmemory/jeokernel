cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)

add_library(files OBJECT directory.cpp fileitem.cpp fsresource.cpp)

target_include_directories(files PRIVATE ../include)

target_include_directories(files PRIVATE ../../../include)
