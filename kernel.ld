ENTRY(bootstrap)

SECTIONS
{
	/* 30bit up - (9+9+12)bit - (pagetable alignment) */
	. = 1026M;

  	/* text section */
	.text ALIGN(4K) : SUBALIGN(8)
	{
	    __text_start = .;
		*(.multiboot)
		*(.text)
	}

	/* read only data section */
	.rodata ALIGN(4K) : SUBALIGN(8)
	{
		*(.rodata)
	}

	/* data section */
	.data ALIGN(4K) : SUBALIGN(8)
	{
	    __data_start = .;
		*(.data);
		__data_end = .;
	}

	/* bss section */
	.bss ALIGN(4K) : SUBALIGN(8)
	{
		*(COMMON)
		*(.bss)
	}

	__end = .;

}
