include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(pci OBJECT pci.cpp pci.h)

target_include_directories(pci PRIVATE ../../include)
