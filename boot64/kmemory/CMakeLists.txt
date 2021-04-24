set(CMAKE_CXX_STANDARD 20)

include(../toolchain.cmake)

add_library(kmemory STATIC mallocator.cpp mallocator.h malloc.cpp)

target_include_directories(kmemory PRIVATE ../../include)
