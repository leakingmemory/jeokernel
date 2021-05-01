.section .text
.code32

#define MB_KERNEL_MAGIC 0x1BADB002
#define MB_HFLAG_MODALIGN   (1<<0)      /* Align loaded modules on page boundaries */
#define MB_HFLAG_MEMINFO   (1<<1)      /* Memory map */
#define MB_HFLAG_KLUDGE      (1<<16)      /* Use a.out kludge */

#define MULTIBOOT2_HEADER_MAGIC                 0xe85250d6
#define MULTIBOOT_ARCHITECTURE_I386  0

#define MAGIC MULTIBOOT2_HEADER_MAGIC
#define ARCH MULTIBOOT_ARCHITECTURE_I386
#define MBSIZE (multiboot_header_end - multiboot_header)
#define CHECK -(MAGIC + ARCH + MBSIZE)

    .align 8
multiboot_header:
    .long MAGIC
    .long ARCH
    .long MBSIZE
    .long CHECK
multiboot_entrypoint:
    .short 3 /* tag entry address */
    .short 1 /* optional */
    .long (multiboot_entrypoint_end - multiboot_entrypoint) /* size */
    .long bootstrap /* entrypoint addr */
multiboot_entrypoint_end:
    .short 0 /* pad */
    .short 0 /* pad */
    /* end tag : */
    .short 0 /* end */
    .short 0
    .long 8 /* size */
multiboot_header_end:
       //.long MB_KERNEL_MAGIC
       //.long (MB_HFLAG_MODALIGN|MB_HFLAG_MEMINFO|MB_HFLAG_KLUDGE)
       //.long -(MB_KERNEL_MAGIC + (MB_HFLAG_MODALIGN|MB_HFLAG_MEMINFO|MB_HFLAG_KLUDGE))
       //.long multiboot_header
       //.long __text_start
       //.long __data_end
       //.long __end
       //.long bootstrap

stackBottom:

.skip 1024

stackTop:

.globl _start
.globl bootstrap

_start:
bootstrap:
    mov $0x400000, %esp
    mov $0x400000, %ebp
    push %ebx
    mov %ebx,%edi
    call boot_stage1
    pop %ebx
    hlt

.globl jumpto64
jumpto64:
    ljmp $0x08,$in64bit
    hlt

.code64
in64bit:
    .short 0xe0ff // jmp %rax
    hlt;
