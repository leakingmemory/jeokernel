/*
 * Link script for the first-stage arm64 boot shim.
 *
 * QEMU "virt" RAM starts at 0x40000000. With image_size=0 in the arm64 header
 * the loader places the image at RAM + 0x80000 = 0x40080000 and runs it with
 * the MMU off, so for now we link to run from that physical address directly
 * (no relocation, no virtual memory yet - that comes in later milestones).
 */

ENTRY(_start)

SECTIONS
{
	. = 0x40080000;

	.text : ALIGN(4K) {
		KEEP(*(.text.boot))	/* arm64 header must stay at offset 0 */
		*(.text*)
	}

	.rodata : ALIGN(4K) {
		*(.rodata*)
	}

	.data : ALIGN(4K) {
		*(.data*)
	}

	.bss : ALIGN(4K) {
		__bss_start = .;
		*(COMMON)
		*(.bss*)
		. = ALIGN(16);
		__bss_end = .;
	}

	/* 16 KiB boot stack, grows down from __stack_top */
	. = ALIGN(16);
	. += 0x4000;
	__stack_top = .;

	__end = .;

	/DISCARD/ : {
		*(.note*)
		*(.comment)
		*(.eh_frame*)
	}
}
