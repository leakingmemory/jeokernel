include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 23)

add_library(clock OBJECT clock.cpp)

target_include_directories(clock PRIVATE ../../include)
