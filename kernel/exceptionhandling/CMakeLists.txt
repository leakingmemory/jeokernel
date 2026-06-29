include(../arch/x86_64/toolchain.cmake)

set(CMAKE_CXX_STANDARD 23)

add_library(exceptionhandling OBJECT unwind.cpp)

target_include_directories(exceptionhandling PRIVATE ../../include)
