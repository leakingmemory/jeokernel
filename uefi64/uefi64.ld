ENTRY(efi_main)

SECTIONS
{
	. = 0x400000;
	.text :
	{
		__text_start = .;
		*(.text)
		*(.text.*)
		*(.gnu.linkonce.t.*)
		__text_end = .;
	}

	.rodata ALIGN(4096) :
	{
		__rodata_start = .;
		*(.rodata)
		*(.rodata.*)
		*(.gnu.linkonce.r.*)
		__rodata_end = .;
	}

	.data ALIGN(4096) :
	{
		__data_start = .;
		*(.data)
		*(.data.*)
		*(.gnu.linkonce.d.*)
		__data_end = .;
	}

	.sdata ALIGN(4096) :
	{
		__sdata_start = .;
		*(.sdata)
		*(.sdata.*)
		__sdata_end = .;
	}

	.bss ALIGN(4096) :
	{
		__bss_start = .;
		*(.sbss)
		*(.scommon)
		*(.bss)
		*(.bss.*)
		*(COMMON)
		__bss_end = .;
	}

	.reloc ALIGN(4096) :
	{
		*(.reloc)
	}

	.dynamic ALIGN(4096) :
	{
		*(.dynamic)
	}

	/DISCARD/ :
	{
		*(.note.GNU-stack)
		*(.comment)
	}
}
