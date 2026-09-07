if (ARCH STREQUAL "aarch64")
    include(../arch/aarch64/toolchain.cmake)
else()
    include(../arch/x86_64/toolchain.cmake)
endif()

set(CMAKE_CXX_STANDARD 23)

if (ARCH STREQUAL "aarch64")
    add_library(kmemory OBJECT malloc.cpp)
else()
    add_library(kmemory OBJECT mallocator.cpp mallocator.h malloc.cpp ChainedAllocator.cpp ChainedAllocator.h ChainedAllocatorRoot.cpp ChainedAllocatorRoot.h)
endif()

target_include_directories(kmemory PRIVATE ../../include)
