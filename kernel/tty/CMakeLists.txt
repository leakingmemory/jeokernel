include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(dev)

add_library(tty OBJECT tty.cpp ttyhandler.cpp ttyhandler.h)

target_include_directories(tty PRIVATE ../../include)
