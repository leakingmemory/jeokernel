include(../../toolchain.cmake)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DACPI_MACHINE_WIDTH=64 -D_LINUX -DACPI_USE_SYSTEM_CLIBRARY")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DACPI_MACHINE_WIDTH=64 -D_LINUX -DACPI_USE_SYSTEM_CLIBRARY")

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(acpica-unix-20210604/source/common)
add_subdirectory(acpica-unix-20210604/source/components/dispatcher)
add_subdirectory(acpica-unix-20210604/source/components/events)
add_subdirectory(acpica-unix-20210604/source/components/executer)
add_subdirectory(acpica-unix-20210604/source/components/hardware)
add_subdirectory(acpica-unix-20210604/source/components/namespace)
add_subdirectory(acpica-unix-20210604/source/components/parser)
add_subdirectory(acpica-unix-20210604/source/components/resources)
add_subdirectory(acpica-unix-20210604/source/components/tables)
add_subdirectory(acpica-unix-20210604/source/components/utilities)

add_library(acpica STATIC acpica.cpp acpica_impl.cpp acpica_c.c)

target_link_libraries(acpica PRIVATE acpica-common)
target_link_libraries(acpica PRIVATE acpica-dispatcher)
target_link_libraries(acpica PRIVATE acpica-events)
target_link_libraries(acpica PRIVATE acpica-executer)
target_link_libraries(acpica PRIVATE acpica-hardware)
target_link_libraries(acpica PRIVATE acpica-namespace)
target_link_libraries(acpica PRIVATE acpica-parser)
target_link_libraries(acpica PRIVATE acpica-resources)
target_link_libraries(acpica PRIVATE acpica-tables)
target_link_libraries(acpica PRIVATE acpica-utilities)

target_include_directories(acpica PUBLIC ../../../include)
target_include_directories(acpica PRIVATE acpica-unix-20210604/source/include)
