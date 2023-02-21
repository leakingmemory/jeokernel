set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

function(detect_clang)
        foreach(perhaps_clangpp IN LISTS ARGN)
                string(REPLACE "++" "" perhaps_clang ${perhaps_clangpp})
                message("Looking for ${perhaps_clangpp} and ${perhaps_clang}")
                execute_process(COMMAND ${perhaps_clang} "--version" RESULT_VARIABLE clang_is_avail)
                execute_process(COMMAND ${perhaps_clangpp} "--version" RESULT_VARIABLE clangpp_is_avail)
                if (${clang_is_avail} EQUAL "0" AND ${clangpp_is_avail} EQUAL "0")
                        message("Found ${perhaps_clangpp} and ${perhaps_clang}")
                        set(CMAKE_C_COMPILER ${perhaps_clang} PARENT_SCOPE)
                        set(CMAKE_CXX_COMPILER ${perhaps_clangpp} PARENT_SCOPE)
                        set(CMAKE_LINKER "ld.lld" PARENT_SCOPE)
                        return()
                endif()
        endforeach()
        message(FATAL_ERROR "clang was not found")
endfunction()

if(DEFINED ENV{USE_CLANG})
        set(USE_CLANG true)

        detect_clang("clang++-15" "clang++15" "clang++-14" "clang++14" "clang++-13" "clang++13" "clang++-12" "clang++12" "clang++-11" "clang++11" "clang++")
endif(DEFINED ENV{USE_CLANG})

cmake_minimum_required(VERSION 3.16)
project(jeokernel)

set(CMAKE_CXX_STANDARD 20)

enable_language(C ASM)

add_subdirectory(boot32)
add_subdirectory(kernel)

if (NOT DEFINED ENV{DISABLE_TESTS})
        enable_testing()
        add_subdirectory(test)
endif (NOT DEFINED ENV{DISABLE_TESTS})

#add_executable(jeokernel /dev/null)
#target_link_libraries(jeokernel boot32)
#target_link_libraries(jeokernel kernel)

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

add_custom_command(
        OUTPUT jeokernelefi.img
        DEPENDS jeokernel kernel
        COMMAND env CMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}" ${CMAKE_SOURCE_DIR}/create_efiboot.sh
)

add_custom_target(jeokernel_disk ALL echo "ISO file built" DEPENDS jeokernel.img)

IF(NOT (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD"))
        add_custom_target(jeokernel_iso ALL echo "ISO file built" DEPENDS jeokernel.iso)
        add_custom_target(jeokernel_efi ALL echo "ISO file built" DEPENDS jeokernelefi.img)
ENDIF()
