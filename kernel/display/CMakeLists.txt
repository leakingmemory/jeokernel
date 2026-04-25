include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 23)

add_library(display OBJECT vga.cpp vga.h)

target_include_directories(display PRIVATE ../../include)
