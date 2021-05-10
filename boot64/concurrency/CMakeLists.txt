include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(concurrency OBJECT critical_section.cpp hw_spinlock.cpp)

target_include_directories(concurrency PRIVATE ../../include)
