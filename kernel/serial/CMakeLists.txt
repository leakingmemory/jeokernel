if (ARCH STREQUAL "aarch64")
    include(../arch/aarch64/toolchain.cmake)
else()
    include(../arch/x86_64/toolchain.cmake)
endif()

set(CMAKE_CXX_STANDARD 23)

if (ARCH STREQUAL "aarch64")
    add_library(serial OBJECT serialtty.cpp serialtty.h uart.cpp uart.h)
else()
    add_library(serial OBJECT serialport.cpp serialport.h serialtty.cpp serialtty.h)
endif()

target_include_directories(serial PRIVATE ../../include)
