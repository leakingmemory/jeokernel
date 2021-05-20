include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(acpi OBJECT ACPI.cpp)

target_include_directories(acpi PRIVATE ../../include)
