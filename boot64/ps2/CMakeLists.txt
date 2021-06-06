include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(ps2 OBJECT ps2.cpp ps2.h)

target_include_directories(ps2 PRIVATE ../../include)
