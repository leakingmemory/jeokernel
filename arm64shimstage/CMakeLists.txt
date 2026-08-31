include(toolchain.cmake)

cmake_minimum_required(VERSION 3.16)
project(jeokernel_uefi)

set(CMAKE_CXX_STANDARD 23)

enable_language(C ASM)

add_executable(arm64shimstage arm64shimstage.cpp)

target_include_directories(arm64shimstage PRIVATE ../include)
