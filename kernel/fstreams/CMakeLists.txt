if (ARCH STREQUAL "aarch64")
    include(../arch/aarch64/toolchain.cmake)
else()
    include(../arch/x86_64/toolchain.cmake)
endif()

set(CMAKE_CXX_STANDARD 23)

add_library(fstreams OBJECT fstreams.cpp stdout.cpp stdin.cpp iostream.cpp)

target_include_directories(fstreams PRIVATE ../../include)
