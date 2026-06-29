# Cross-compilation flags for the aarch64 kernel, included from kernel's
# CMakeLists.txt - the same pattern arm64shim/ uses. clang is aimed at aarch64
# and the executable is linked with kernel.ld (found relative to this file, so
# the include works from any directory) via lld. Relies on the top-level build
# having selected clang (USE_CLANG / -DARCH=aarch64).

set(triple aarch64-none-elf)

# -mgeneral-regs-only keeps the compiler from emitting NEON/FP in kernel-side
# code, so we don't have to save/restore SIMD state on the interrupt path later.
set(archflags "--target=${triple} -mgeneral-regs-only -nostdinc -nostdlib -ffreestanding -fno-stack-protector -fno-builtin -fno-PIC -fno-exceptions -fno-rtti")

set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${archflags}")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${archflags}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${archflags}")

set(CMAKE_CXX_LINK_EXECUTABLE
	"${CMAKE_CXX_COMPILER} --target=${triple} -nostdlib -fuse-ld=lld -Wl,-no-pie -Wl,--build-id=none -T ${CMAKE_CURRENT_LIST_DIR}/kernel.ld <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
