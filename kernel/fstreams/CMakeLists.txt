include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(fstreams OBJECT fstreams.cpp stdout.cpp stdin.cpp)

target_include_directories(fstreams PRIVATE ../../include)
