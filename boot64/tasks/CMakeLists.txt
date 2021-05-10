include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(tasks OBJECT tasklist.cpp thread.cpp)

target_include_directories(tasks PRIVATE ../../include)
