# Cross-compilation flags for the arm64 boot shim, applied to this subdirectory
# only via include() from arm64shim/CMakeLists.txt - exactly like
# uefi64stage/toolchain.cmake. clang is a native cross-compiler, so the main
# build's clang/clang++ just needs to be aimed at aarch64 with a few flags; no
# separate toolchain file or compiler binary is required. This relies on the
# top-level build having selected clang, so build with USE_CLANG set
# (jeokernel-builds/clang-arm64.sh does this for you).

set(triple aarch64-none-elf)

# -mgeneral-regs-only keeps the compiler from emitting NEON/FP in kernel-side
# code, so we don't have to save/restore SIMD state on the interrupt path later.
set(archflags "--target=${triple} -mgeneral-regs-only -nostdinc -nostdlib -ffreestanding -fno-stack-protector -fno-builtin -fno-PIC -fno-exceptions -fno-rtti")

set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${archflags}")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${archflags}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${archflags}")

set(CMAKE_CXX_LINK_EXECUTABLE
	"${CMAKE_CXX_COMPILER} --target=${triple} -nostdlib -fuse-ld=lld -Wl,-no-pie -Wl,--build-id=none -T ${CMAKE_SOURCE_DIR}/arm64shim/arm64shim.ld <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
