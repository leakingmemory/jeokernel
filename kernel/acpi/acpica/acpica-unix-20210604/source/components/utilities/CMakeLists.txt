include(../../../../../../toolchain.cmake)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DACPI_MACHINE_WIDTH=64 -D_LINUX -DACPI_USE_SYSTEM_CLIBRARY -DACPI_DEBUGGER -DACPI_USE_LOCAL_CACHE")


set(CMAKE_CXX_STANDARD 20)

AUX_SOURCE_DIRECTORY(. C_SOURCES)

add_library(acpica-utilities OBJECT ${C_SOURCES})

target_include_directories(acpica-utilities PRIVATE ../../compiler)
target_include_directories(acpica-utilities PRIVATE ../../include)
target_include_directories(acpica-utilities PRIVATE ../../../../../../../include)
