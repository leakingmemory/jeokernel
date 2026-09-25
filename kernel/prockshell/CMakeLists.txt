include(../arch/x86_64/toolchain.cmake)

set(CMAKE_CXX_STANDARD 23)

add_library(prockshell OBJECT prockshell.cpp kshell_ups.cpp kshell_ups.h)

target_include_directories(prockshell PRIVATE ../../include)
if (ARCH STREQUAL "x86_64")
    target_include_directories(prockshell PRIVATE ../arch/x86_64/include)
endif()