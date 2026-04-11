set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

include(toolchain.cmake)

cmake_minimum_required(VERSION 3.16)
project(jeokernel_uefi)

set(CMAKE_CXX_STANDARD 20)

enable_language(C ASM)

    if(USE_CLANG)
        set(UEFI64_SHIM_TARGET jeokernel_shim.efi)
    else(USE_CLANG)
        set(UEFI64_SHIM_TARGET  jeokernel_shim)
    endif(USE_CLANG)

add_executable(${UEFI64_SHIM_TARGET} efi_main.cpp wrapbin.S)

# Ensure the kernel binary exists before building the shim
add_dependencies(${UEFI64_SHIM_TARGET} kernel)

# To allow wrapbin.S to find the kernel binary via .incbin
# We'll pass the path as a preprocessor definition to the assembler
get_target_property(KERNEL_BINARY_DIR kernel BINARY_DIR)
target_compile_options(${UEFI64_SHIM_TARGET} PRIVATE "$<$<COMPILE_LANGUAGE:ASM>:-DKERNEL_PATH=\"${CMAKE_BINARY_DIR}/kernel/kernel\">")

target_include_directories(${UEFI64_SHIM_TARGET} PRIVATE ../include)

if (NOT USE_CLANG)
    add_custom_command(
            TARGET ${UEFI64_SHIM_TARGET} POST_BUILD
            COMMAND objcopy -j .text -j .rodata -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rel.dyn -j .reloc -O pei-x86-64 jeokernel_shim jeokernel_shim.efi
            COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/set_dynamic_base.py ${CMAKE_CURRENT_BINARY_DIR}/jeokernel_shim.efi
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating PE/COFF UEFI binary jeokernel_shim.efi and setting DYNAMIC_BASE"
    )
endif()
