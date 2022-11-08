set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

if(DEFINED ENV{USE_CLANG})
        set(USE_CLANG true)

        execute_process(COMMAND "clang++-15" "--version" RESULT_VARIABLE clang15_avail)

        if (${clang15_avail} EQUAL "0")
                message("Found clang 15")
                set(CMAKE_C_COMPILER clang-15)
                set(CMAKE_CXX_COMPILER clang++-15)
        else()

                execute_process(COMMAND "clang++-14" "--version" RESULT_VARIABLE clang14_avail)

                if (${clang14_avail} EQUAL "0")
                        message("Found clang 14")
                        set(CMAKE_C_COMPILER clang-14)
                        set(CMAKE_CXX_COMPILER clang++-14)
                else()
                        execute_process(COMMAND "clang++-13" "--version" RESULT_VARIABLE clang13_avail)

                        if (${clang13_avail} EQUAL "0")
                                message("Found clang 13")
                                set(CMAKE_C_COMPILER clang-13)
                                set(CMAKE_CXX_COMPILER clang++-13)
                        else()
                                execute_process(COMMAND "clang++-12" "--version" RESULT_VARIABLE clang12_avail)

                                if (${clang12_avail} EQUAL "0")
                                        message("Found clang 12")
                                        set(CMAKE_C_COMPILER clang-12)
                                        set(CMAKE_CXX_COMPILER clang++-12)
                                else()
                                        message("Trying clang 11")
                                        set(CMAKE_C_COMPILER clang-11)
                                        set(CMAKE_CXX_COMPILER clang++-11)
                                endif()
                        endif()
                endif()
        endif()
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

add_custom_target(jeokernel_iso ALL echo "ISO file built" DEPENDS jeokernel.iso)
add_custom_target(jeokernel_disk ALL echo "ISO file built" DEPENDS jeokernel.img)
add_custom_target(jeokernel_efi ALL echo "ISO file built" DEPENDS jeokernelefi.img)
