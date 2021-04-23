set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -march=x86-64 -nostdinc -nostdlib")

add_library(kmemory STATIC mallocator.cpp mallocator.h malloc.cpp)

target_include_directories(kmemory PRIVATE ../../include)
