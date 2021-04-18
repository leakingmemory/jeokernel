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
	} > ram

	/* read only data section */
	.rodata BLOCK(4K) : ALIGN(4K)
	{
		*(.rodata)
	} > ram

	/* data section */
	.data BLOCK(4K) : ALIGN(4K)
	{
	    __data_start = .;
		*(.data);
		__data_end = .;
	} > ram

	/* bss section */
	.bss BLOCK(4K) : ALIGN(4K)
	{
		*(COMMON)
		*(.bss)
	} > ram

	__end = .;

}