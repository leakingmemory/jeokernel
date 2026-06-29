/*
 * Link script for the (placeholder) aarch64 kernel.
 *
 * The kernel does nothing yet and is only embedded into the arm64 shim, so the
 * load address below is a provisional choice above the shim's 0x40080000 image.
 * It will firm up once the shim actually loads and jumps to the kernel.
 */

ENTRY(_start)

SECTIONS
{
	. = 0x40200000;

	.text : ALIGN(4K) {
		KEEP(*(.text.boot))
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
