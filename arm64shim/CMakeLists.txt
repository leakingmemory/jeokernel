# The arm64 boot shim is cross-compiled for aarch64. toolchain.cmake (included
# the same way uefi64stage does) aims this subdirectory's clang at aarch64; the
# top-level CMakeLists.txt pulls us in when -DBUILD_ARM64SHIM=ON and skips the
# x86_64 kernel, boot loaders and tests. See jeokernel-builds/clang-arm64.sh.
include(toolchain.cmake)

add_executable(arm64shim.elf head.S shim.cpp)
target_compile_options(arm64shim.elf PRIVATE -g -Wall -Wextra)

# The loader consumes a flat "Image", not an ELF, so strip the ELF down to a
# raw binary. Keep the .elf around for gdb (qemu -s -S; gdb arm64shim.elf).
add_custom_command(
	OUTPUT arm64shim.bin
	COMMAND llvm-objcopy -O binary $<TARGET_FILE:arm64shim.elf> arm64shim.bin
	DEPENDS arm64shim.elf
	COMMENT "objcopy arm64shim.elf -> flat boot image arm64shim.bin")

add_custom_target(arm64shim ALL DEPENDS arm64shim.bin)
