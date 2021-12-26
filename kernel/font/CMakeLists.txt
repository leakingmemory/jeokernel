include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)


add_library(font OBJECT font8x8.cpp)

target_include_directories(font PRIVATE ../../include)
