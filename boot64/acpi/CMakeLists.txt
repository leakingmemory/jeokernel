include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(acpica)

add_library(acpi OBJECT ACPI.cpp)

target_link_libraries(acpi PUBLIC acpica)

target_include_directories(acpi PRIVATE ../../include)
