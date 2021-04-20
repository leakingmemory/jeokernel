cmake_minimum_required(VERSION 3.16)
project(jeokernel)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -march=x86-64 -nostdinc -nostdlib")

add_library(textconsole STATIC b8000.cpp b8000.h)

target_include_directories(textconsole PRIVATE ../../include)
