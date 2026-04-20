.section .rodata
.global wrapped_binary
wrapped_binary:
.incbin KERNEL_PATH
.global wrapped_uefistage_start
wrapped_uefistage_start:
.incbin UEFI64STAGE_PATH
.global wrapped_uefistage_end
wrapped_uefistage_end:
