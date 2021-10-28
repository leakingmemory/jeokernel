include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(keyboard OBJECT keyboard.cpp)

target_include_directories(keyboard PRIVATE ../../include)
