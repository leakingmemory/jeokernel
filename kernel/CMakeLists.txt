set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

# Aim the compiler at the target architecture: aarch64 uses the cross toolchain
# (arch/aarch64/toolchain.cmake, linking with arch/aarch64/kernel.ld); x86_64
# uses arch/x86_64/toolchain.cmake.
if (ARCH STREQUAL "aarch64")
        include(arch/aarch64/toolchain.cmake)
else()
        include(arch/x86_64/toolchain.cmake)
endif()

set(CMAKE_CXX_STANDARD 23)

enable_language(C ASM)

# Subsystem libraries. x86_64-only for now; as each is ported to aarch64 it
# moves out of this guard to be shared by both architectures.
if (ARCH STREQUAL "x86_64")
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
add_subdirectory(keyboard)
add_subdirectory(strutil)
add_subdirectory(fstreams)
add_subdirectory(framebuffer)
add_subdirectory(font)
add_subdirectory(kshell)
add_subdirectory(scsi)
add_subdirectory(blockdevs/blockdevs)
add_subdirectory(blockdevs/hashfunc)
add_subdirectory(blockdevs/files)
add_subdirectory(blockdevs/filesystems)
add_subdirectory(blockdevsystem)
add_subdirectory(libcxxrt/src)
add_subdirectory(exceptionhandling)
add_subdirectory(kfs)
add_subdirectory(exec)
add_subdirectory(syscall)
add_subdirectory(sha2)
add_subdirectory(random)
add_subdirectory(clock)
add_subdirectory(devfs)
add_subdirectory(tty)
add_subdirectory(procfs)
add_subdirectory(resource)
add_subdirectory(prockshell)
add_subdirectory(serial)

add_library(bootstrap arch/x86_64/bootstrap.S)
endif()

# The kernel image - same target name (kernel.full) on both architectures, with
# architecture-specific sources; the loader/shim embeds this target's file.
if (ARCH STREQUAL "aarch64")
add_executable(kernel.full arch/aarch64/bootstrap.cpp)
target_compile_options(kernel.full PRIVATE -g -Wall -Wextra)
else()
add_executable(kernel.full m64-stage1.cpp arch/x86_64/bootstrap.cpp cpuio.cpp pagetable.cpp pagealloc.cpp cpp_handlers.cpp strings.cpp klogger.cpp stack.cpp GlobalDescriptorTable.cpp GlobalDescriptorTable.h InterruptDescriptorTable.cpp InterruptDescriptorTable.h TaskStateSegment.cpp TaskStateSegment.h InterruptTaskState.cpp InterruptTaskState.h interrupt.S interrupt.cpp PageFault.cpp PageFault.h elf.cpp KernelElf.cpp KernelElf.h vmem.cpp cpu_mpfp.cpp LocalApic.cpp Pic.cpp Pic.h IOApic.cpp IOApic.h HardwareInterrupts.cpp HardwareInterrupts.h PITTimerCalib.cpp PITTimerCalib.h ap_trampoline.S start_ap.cpp start_ap.h ap_started.S ap_bootstrap.cpp nanotime.cpp AcpiBoot.cpp AcpiBoot.h link_stubs.cpp ctime.cpp perror.cpp CpuInterrupts.cpp CpuInterrupts.h remove.cpp ApStartup.cpp ApStartup.h DoubleFault.cpp DoubleFault.h statistics_root.cpp physpagemap.cpp dynlinker.cpp PerCpuVMem.cpp PerCpuVMem.cpp SyscallSupport.cpp SyscallSupport.h uname_info.cpp x86fpu.cpp)
endif()

# Shared by both architectures.
target_include_directories(kernel.full PRIVATE ../include)

# Split the debug info out into kernel.debug and produce a stripped "kernel"
# image (which is what the loader/shim embeds). Both arches do this; x86_64 uses
# the host objcopy, aarch64 the llvm one (objcopy may not grok aarch64 ELFs).
if (ARCH STREQUAL "aarch64")
        set(OBJCOPY llvm-objcopy)
else()
        set(OBJCOPY objcopy)
endif()

add_custom_target(
        kernel.debug
        ALL ${OBJCOPY} --only-keep-debug kernel.full kernel.debug
        DEPENDS kernel.full
)

add_custom_target(
        kernel
        ALL ${OBJCOPY} --strip-debug --add-gnu-debuglink=kernel.debug kernel.full kernel
        DEPENDS kernel.full kernel.debug
)

# x86_64-only subsystem linkage; becomes shared as subsystems are ported.
if (ARCH STREQUAL "x86_64")
target_link_libraries(kernel.full PRIVATE bootstrap)
target_link_libraries(kernel.full PRIVATE textconsole)
target_link_libraries(kernel.full PRIVATE kmemory)
target_link_libraries(kernel.full PRIVATE devices)
target_link_libraries(kernel.full PRIVATE pci)
target_link_libraries(kernel.full PRIVATE concurrency)
target_link_libraries(kernel.full PRIVATE tasks)
target_link_libraries(kernel.full PRIVATE acpi)
target_link_libraries(kernel.full PRIVATE display)
target_link_libraries(kernel.full PRIVATE usb)
target_link_libraries(kernel.full PRIVATE ps2)
target_link_libraries(kernel.full PRIVATE keyboard)
target_link_libraries(kernel.full PRIVATE strutil)
target_link_libraries(kernel.full PRIVATE fstreams)
target_link_libraries(kernel.full PRIVATE framebuffer)
target_link_libraries(kernel.full PRIVATE font)
target_link_libraries(kernel.full PRIVATE kshell)
target_link_libraries(kernel.full PRIVATE scsi)
target_link_libraries(kernel.full PRIVATE blockdevs)
target_link_libraries(kernel.full PRIVATE hashfunc)
target_link_libraries(kernel.full PRIVATE files)
target_link_libraries(kernel.full PRIVATE filesystems)
target_link_libraries(kernel.full PRIVATE blockdevsystem)
target_link_libraries(kernel.full PRIVATE cxxrt-static)
target_link_libraries(kernel.full PRIVATE exceptionhandling)
target_link_libraries(kernel.full PRIVATE kfs)
target_link_libraries(kernel.full PRIVATE exec)
target_link_libraries(kernel.full PRIVATE syscall)
target_link_libraries(kernel.full PRIVATE syscall_impl)
target_link_libraries(kernel.full PRIVATE sha2)
target_link_libraries(kernel.full PRIVATE random)
target_link_libraries(kernel.full PRIVATE clock)
target_link_libraries(kernel.full PRIVATE devfs)
target_link_libraries(kernel.full PRIVATE ttydev)
target_link_libraries(kernel.full PRIVATE tty)
target_link_libraries(kernel.full PRIVATE procfs)
target_link_libraries(kernel.full PRIVATE resource)
target_link_libraries(kernel.full PRIVATE prockshell)
target_link_libraries(kernel.full PRIVATE serial)
endif()
