include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(exec OBJECT exec.cpp process.cpp UserElf.cpp UserElf.h)

target_include_directories(exec PRIVATE ../../include)
