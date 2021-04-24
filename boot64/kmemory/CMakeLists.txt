include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(kmemory STATIC mallocator.cpp mallocator.h malloc.cpp)

target_include_directories(kmemory PRIVATE ../../include)
