include(../arch/x86_64/toolchain.cmake)

set(CMAKE_CXX_STANDARD 23)

add_library(devices OBJECT devices.cpp drivers.cpp device_group_stats.cpp device_group_stats.h)

target_include_directories(devices PRIVATE ../../include)
if (ARCH STREQUAL "x86_64")
    target_include_directories(devices PRIVATE ../arch/x86_64/include)
endif()