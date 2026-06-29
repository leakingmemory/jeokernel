/*
 * Embed the aarch64 kernel image into the shim, the same mechanism
 * uefi64/wrapbin.S uses to embed the x86_64 kernel into the UEFI loader. The
 * path comes from the KERNEL_PATH define set by CMakeLists.txt. The shim will
 * hand control to the embedded kernel in a later milestone; for now it is just
 * carried along in .rodata, bracketed by these symbols.
 */
.section .rodata
.global wrapped_kernel_start
wrapped_kernel_start:
.incbin KERNEL_PATH
.global wrapped_kernel_end
wrapped_kernel_end:
