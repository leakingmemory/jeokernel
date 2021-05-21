include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(devices OBJECT devices.cpp)

target_include_directories(devices PRIVATE ../../include)
