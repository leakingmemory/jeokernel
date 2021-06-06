set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

include(toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

enable_language(C ASM)

add_subdirectory(textconsole)
add_subdirectory(kmemory)
add_subdirectory(devices)
add_subdirectory(pci)
add_subdirectory(concurrency)
add_subdirectory(tasks)
add_subdirectory(acpi)
add_subdirectory(display)
add_subdirectory(usb)
add_subdirectory(ps2)

add_library(bootstrap bootstrap.S)

add_executable(kernel m64-stage1.cpp cpuio.cpp pagetable.cpp pagealloc.cpp cpp_handlers.cpp strings.cpp klogger.cpp stack.cpp GlobalDescriptorTable.cpp GlobalDescriptorTable.h InterruptDescriptorTable.cpp InterruptDescriptorTable.h TaskStateSegment.cpp TaskStateSegment.h InterruptTaskState.cpp InterruptTaskState.h interrupt.S interrupt.cpp PageFault.cpp PageFault.h elf.cpp KernelElf.cpp KernelElf.h vmem.cpp cpu_mpfp.cpp LocalApic.cpp Pic.cpp Pic.h IOApic.cpp IOApic.h HardwareInterrupts.cpp HardwareInterrupts.h PITTimerCalib.cpp PITTimerCalib.h ap_trampoline.S start_ap.cpp start_ap.h ap_started.S ap_bootstrap.cpp nanotime.cpp AcpiBoot.cpp AcpiBoot.h)

target_include_directories(kernel PRIVATE ../include)

target_link_libraries(kernel PRIVATE bootstrap)
target_link_libraries(kernel PRIVATE textconsole)
target_link_libraries(kernel PRIVATE kmemory)
target_link_libraries(kernel PRIVATE devices)
target_link_libraries(kernel PRIVATE pci)
target_link_libraries(kernel PRIVATE concurrency)
target_link_libraries(kernel PRIVATE tasks)
target_link_libraries(kernel PRIVATE acpi)
target_link_libraries(kernel PRIVATE display)
target_link_libraries(kernel PRIVATE usb)
target_link_libraries(kernel PRIVATE ps2)

