set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

include(toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

enable_language(C ASM)

add_subdirectory(textconsole)
add_subdirectory(kmemory)

add_library(bootstrap bootstrap.S)

add_executable(kernel m64-stage1.cpp cpuio.cpp pagetable.cpp pagealloc.cpp cpp_handlers.cpp strings.cpp klogger.cpp stack.cpp GlobalDescriptorTable.cpp GlobalDescriptorTable.h InterruptDescriptorTable.cpp InterruptDescriptorTable.h TaskStateSegment.cpp TaskStateSegment.h InterruptTaskState.cpp InterruptTaskState.h interrupt.S interrupt.cpp PageFault.cpp PageFault.h elf.cpp KernelElf.cpp KernelElf.h vmem.cpp vmem.h cpu_mpfp.cpp cpu_mpfp.h)

target_include_directories(kernel PRIVATE ../include)

target_link_libraries(kernel PRIVATE bootstrap)
target_link_libraries(kernel PRIVATE textconsole)
target_link_libraries(kernel PRIVATE kmemory)
