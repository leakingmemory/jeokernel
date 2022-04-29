include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(exceptionhandling OBJECT unwind.cpp)

target_include_directories(exceptionhandling PRIVATE ../../include)
