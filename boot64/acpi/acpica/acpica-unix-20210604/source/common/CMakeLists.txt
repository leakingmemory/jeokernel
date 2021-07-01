include(../../../../../toolchain.cmake)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DACPI_MACHINE_WIDTH=64 -D_LINUX -DACPI_USE_SYSTEM_CLIBRARY -DACPI_DISASSEMBLER -DACPI_ASL_COMPILER")


set(CMAKE_CXX_STANDARD 20)

AUX_SOURCE_DIRECTORY(. C_SOURCES)
list(REMOVE_ITEM C_SOURCES ./acfileio.c)
list(REMOVE_ITEM C_SOURCES ./adfile.c)
list(REMOVE_ITEM C_SOURCES ./adisasm.c)
list(REMOVE_ITEM C_SOURCES ./cmfsize.c)

add_library(acpica-common OBJECT ${C_SOURCES})

target_include_directories(acpica-common PRIVATE ../compiler)
target_include_directories(acpica-common PRIVATE ../include)
target_include_directories(acpica-common PRIVATE ../../../../../../include)
