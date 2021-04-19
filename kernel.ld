ENTRY(bootstrap)

SECTIONS
{
	/* we need 1MB of space atleast */
	. = 1536K;

  	/* text section */
	.text BLOCK(4K) : ALIGN(4K)
	{
	    __text_start = .;
		*(.multiboot)
		*(.text)
	}

	/* read only data section */
	.rodata BLOCK(4K) : ALIGN(4K)
	{
		*(.rodata)
	}

	/* data section */
	.data BLOCK(4K) : ALIGN(4K)
	{
	    __data_start = .;
		*(.data);
		__data_end = .;
	}

	/* bss section */
	.bss BLOCK(4K) : ALIGN(4K)
	{
		*(COMMON)
		*(.bss)
	}

	__end = .;

}
