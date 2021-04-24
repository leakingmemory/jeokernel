cmake_minimum_required(VERSION 3.16)
project(jeokernel)

set(CMAKE_CXX_STANDARD 20)

include(../toolchain.cmake)

add_library(textconsole STATIC b8000.cpp b8000.h b8000logger.cpp b8000logger.h)

target_include_directories(textconsole PRIVATE ../../include)
