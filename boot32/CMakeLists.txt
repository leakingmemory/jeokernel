set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

include(toolchain.cmake)

cmake_minimum_required(VERSION 3.16)
project(jeokernel)

set(CMAKE_CXX_STANDARD 20)

enable_language(C ASM)

#set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m32 -nostdinc -nostdlib -DIA32 -DLOADER ")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32 -nostdinc -nostdlib -DIA32 -DLOADER ")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32 -nostdinc -nostdlib -DIA32 -DLOADER ")

add_executable(jeokernel bootstrap.S stage1.cpp plainvga32.cpp plainvga32.h strs.cpp ../include/strs.h strings.cpp )

target_include_directories(jeokernel PRIVATE ../include)

#set(CMAKE_CXX_LINK_EXECUTABLE "ld -m elf_i386 -nostdlib -T ${CMAKE_SOURCE_DIR}/boot32.ld <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
