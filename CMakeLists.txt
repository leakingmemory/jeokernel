cmake_minimum_required(VERSION 3.16)

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

# Select the clang/lld toolchain instead of the gcc cross toolchain. Prefer the
# CMake option (-DUSE_CLANG=ON); the USE_CLANG environment variable is still
# honoured so the existing build scripts keep working. option() runs first to
# pick up -DUSE_CLANG, then the env var overrides it; both must be resolved
# before project() so the compiler is chosen before CMake probes it.
option(USE_CLANG "Build with the clang/lld toolchain instead of gcc" OFF)
if(DEFINED ENV{USE_CLANG})
        set(USE_CLANG ON)
endif()

if(USE_CLANG)
        detect_clang("clang++-23" "clang++23" "clang++-22" "clang++22" "clang++-21" "clang++21" "clang++-20" "clang++20" "clang++-19" "clang++19" "clang++-18" "clang++18" "clang++-17" "clang++17" "clang++-16" "clang++16" "clang++-15" "clang++15" "clang++-14" "clang++14" "clang++-13" "clang++13" "clang++-12" "clang++12" "clang++-11" "clang++11" "clang++")
endif()

project(jeokernel)

set(CMAKE_CXX_STANDARD 23)

enable_language(C ASM)

option(BUILD_ISOBOOT "Build the BIOS ISO image (jeokernel.iso) using boot32 as intermediate loader" ON)
option(BUILD_UEFI64BOOT "Build the UEFI 64-bit image (jeokernel_uefi64.img) using uefi64 and uefi64stage as loader stages" ON)
option(BUILD_ARM64SHIM "Build the cross-compiled arm64 boot shim (arm64shim.bin) instead of the x86_64 kernel; requires -DCMAKE_TOOLCHAIN_FILE=arm64shim/toolchain.cmake" OFF)

# The arm64 shim targets aarch64, which a single CMake configure cannot mix with
# the x86_64 kernel. When it is requested we build only the shim (with the arm64
# toolchain file) and skip the entire x86_64 tree below.
if (BUILD_ARM64SHIM)
        add_subdirectory(arm64shim)
        return()
endif()

add_subdirectory(kernel)

if (BUILD_ISOBOOT)
        add_subdirectory(boot32)
endif()

if (BUILD_UEFI64BOOT)
        add_subdirectory(uefi64stage)
        add_subdirectory(uefi64)
endif()

if (NOT DEFINED ENV{DISABLE_TESTS})
        enable_testing()
        add_subdirectory(test)
endif (NOT DEFINED ENV{DISABLE_TESTS})

#add_executable(jeokernel /dev/null)
#target_link_libraries(jeokernel boot32)
#target_link_libraries(jeokernel kernel)

#set(CMAKE_CXX_LINK_EXECUTABLE "ld -m elf_i386 -nostdlib -T ${CMAKE_SOURCE_DIR}/linker.ld <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")

if(USE_CLANG)
        set(UEFI64_SHIM_TARGET jeokernel_shim.efi)
else(USE_CLANG)
        set(UEFI64_SHIM_TARGET  jeokernel_shim)
endif(USE_CLANG)

if (BUILD_ISOBOOT)
        add_custom_command(
                OUTPUT jeokernel.iso
                DEPENDS jeokernel kernel
                COMMAND env CMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}" ${CMAKE_SOURCE_DIR}/create_isoboot.sh
                )
endif()

if (BUILD_ISOBOOT AND NOT (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD"))
        add_custom_target(jeokernel_iso ALL echo "ISO file built" DEPENDS jeokernel.iso)
endif()

if (BUILD_UEFI64BOOT)
        add_custom_command(
                OUTPUT jeokernel_uefi64.img
                DEPENDS ${UEFI64_SHIM_TARGET}
                COMMAND env CMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}" sh ${CMAKE_SOURCE_DIR}/create_uefi64boot.sh
        )
        add_custom_target(jeokernel_uefi64 ALL echo "UEFI 64-bit image built" DEPENDS jeokernel_uefi64.img)
endif()
