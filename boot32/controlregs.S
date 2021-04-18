.section text
.code32

.global get_cr3
.global set_cr3
.global get_cr4
.global set_cr4

get_cr3:
    mov $1, %eax
    ret

set_cr3:
    movl 4(%ebp), %eax
    movl %eax, %cr3
    ret

get_cr4:
    mov $1, %eax
    ret

set_cr4:
    movl 4(%ebp), %eax
    movl %eax, %cr4
    ret

