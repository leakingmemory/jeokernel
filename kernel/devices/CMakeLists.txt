include(../arch/x86_64/toolchain.cmake)

set(CMAKE_CXX_STANDARD 23)

add_library(devices OBJECT devices.cpp drivers.cpp device_group_stats.cpp device_group_stats.h)

target_include_directories(devices PRIVATE ../../include)
