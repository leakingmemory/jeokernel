include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(concurrency STATIC critical_section.cpp)

target_include_directories(concurrency PRIVATE ../../include)
