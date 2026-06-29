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

# Target architecture. x86_64 builds the kernel and all its boot loaders/tests;
# aarch64 builds the arm64 boot loaders (currently just the shim). A single
# CMake configure cannot mix the two, so the aarch64 build skips the entire
# x86_64 tree below.
set(ARCH "x86_64" CACHE STRING "Target architecture: x86_64 or aarch64")
set_property(CACHE ARCH PROPERTY STRINGS x86_64 aarch64)

# Backwards compat: -DBUILD_ARM64SHIM=ON used to be how the shim was selected;
# honour it by switching to the aarch64 architecture.
if (DEFINED BUILD_ARM64SHIM AND BUILD_ARM64SHIM)
        set(ARCH "aarch64")
endif()

if (NOT (ARCH STREQUAL "x86_64" OR ARCH STREQUAL "aarch64"))
        message(FATAL_ERROR "ARCH must be x86_64 or aarch64 (got '${ARCH}')")
endif()

# arm64 boot loader options. The shim is on by default on aarch64 and off (and
# unused) on x86_64; further aarch64 boot loaders can join it here later.
if (ARCH STREQUAL "aarch64")
        set(BUILD_ARM64SHIM_DEFAULT ON)
else()
        set(BUILD_ARM64SHIM_DEFAULT OFF)
endif()
option(BUILD_ARM64SHIM "Build the cross-compiled arm64 boot shim (aarch64)" ${BUILD_ARM64SHIM_DEFAULT})

# x86_64 boot loader / test options (ignored on aarch64).
option(BUILD_ISOBOOT "Build the BIOS ISO image (jeokernel.iso) using boot32 as intermediate loader" ON)
option(BUILD_UEFI64BOOT "Build the UEFI 64-bit image (jeokernel_uefi64.img) using uefi64 and uefi64stage as loader stages" ON)
# Tests are built by default; disable with -DBUILD_TESTS=OFF. The DISABLE_TESTS
# environment variable is still honoured (set to anything) so existing scripts
# keep working; it overrides the option when present.
option(BUILD_TESTS "Build and register the unit tests" ON)
if(DEFINED ENV{DISABLE_TESTS})
        set(BUILD_TESTS OFF)
endif()

# aarch64: build the kernel and the selected arm64 boot loaders (with the arm64
# toolchain files pulled in by their own CMakeLists) and skip the entire x86_64
# tree below. The shim embeds the kernel image, so the kernel is built first.
if (ARCH STREQUAL "aarch64")
        add_subdirectory(kernel)
        if (BUILD_ARM64SHIM)
                add_subdirectory(arm64shim)
        endif()
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

if (BUILD_TESTS)
        enable_testing()
        add_subdirectory(test)
endif (BUILD_TESTS)

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
