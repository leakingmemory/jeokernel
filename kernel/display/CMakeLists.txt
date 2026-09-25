include(../arch/x86_64/toolchain.cmake)

set(CMAKE_CXX_STANDARD 23)

add_library(display OBJECT vga.cpp vga.h)

target_include_directories(display PRIVATE ../../include)
if (ARCH STREQUAL "x86_64")
    target_include_directories(display PRIVATE ../arch/x86_64/include)
endif()