include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(resource OBJECT resource.cpp)

target_include_directories(resource PRIVATE ../../include)
