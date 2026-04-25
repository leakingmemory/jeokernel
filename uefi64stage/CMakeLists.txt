include(toolchain.cmake)

cmake_minimum_required(VERSION 3.16)
project(jeokernel_uefi)

set(CMAKE_CXX_STANDARD 23)

enable_language(C ASM)

add_executable(uefi64stage uefi64stage.cpp)

target_include_directories(uefi64stage PRIVATE ../include)
