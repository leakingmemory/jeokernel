include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(devices OBJECT devices.cpp drivers.cpp device_group_stats.cpp device_group_stats.h)

target_include_directories(devices PRIVATE ../../include)
