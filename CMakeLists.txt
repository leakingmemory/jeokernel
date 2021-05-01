set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

if(DEFINED ENV{USE_CLANG})
        set(USE_CLANG true)
        set(CMAKE_C_COMPILER clang-11)
        set(CMAKE_CXX_COMPILER clang++-11)
endif(DEFINED ENV{USE_CLANG})

cmake_minimum_required(VERSION 3.16)
project(jeokernel)

set(CMAKE_CXX_STANDARD 20)

enable_language(C ASM)

add_subdirectory(boot32)
add_subdirectory(boot64)

if (NOT DEFINED ENV{DISABLE_TESTS})
        enable_testing()
        add_subdirectory(test)
endif (NOT DEFINED ENV{DISABLE_TESTS})

#add_executable(jeokernel /dev/null)
#target_link_libraries(jeokernel boot32)
#target_link_libraries(jeokernel boot64)

#set(CMAKE_CXX_LINK_EXECUTABLE "ld -m elf_i386 -nostdlib -T ${CMAKE_SOURCE_DIR}/linker.ld <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")

add_custom_command(
        OUTPUT jeokernel.iso
        DEPENDS jeokernel kernel
        COMMAND env CMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}" ${CMAKE_SOURCE_DIR}/create_isoboot.sh
        )

add_custom_command(
        OUTPUT jeokernel.img
        DEPENDS jeokernel kernel
        COMMAND env CMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}" ${CMAKE_SOURCE_DIR}/create_diskboot.sh
)

add_custom_target(jeokernel_iso ALL echo "ISO file built" DEPENDS jeokernel.iso)
add_custom_target(jeokernel_disk ALL echo "ISO file built" DEPENDS jeokernel.img)
