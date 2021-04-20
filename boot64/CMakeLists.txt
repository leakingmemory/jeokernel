cmake_minimum_required(VERSION 3.16)
project(jeokernel)

set(CMAKE_CXX_STANDARD 20)

enable_language(C ASM)

add_subdirectory(textconsole)

set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m64 -march=x86-64 -nostdinc -nostdlib")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64 -march=x86-64 -nostdinc -nostdlib")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -march=x86-64 -nostdinc -nostdlib")

add_executable(kernel bootstrap.S m64-stage1.cpp cpuio.cpp pagetable.cpp pagealloc.cpp)

target_include_directories(kernel PRIVATE ../include)

target_link_libraries(kernel PRIVATE textconsole)

set(CMAKE_CXX_LINK_EXECUTABLE "gcc -Wl,--oformat=elf64-x86-64 -no-pie -nostdlib -T ${CMAKE_SOURCE_DIR}/kernel.ld <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES> -lgcc") # -lsupc++
