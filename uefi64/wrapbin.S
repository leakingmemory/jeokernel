.section .rodata
.global wrapped_binary
wrapped_binary:
.incbin KERNEL_PATH
.global wrapped_binary_end
wrapped_binary_end: