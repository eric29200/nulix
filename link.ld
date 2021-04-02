OUTPUT_FORMAT(elf32-i386)
ENTRY(loader)

SECTIONS {
	kernel_start = 0x100000;
	. = 0x00100000;

	.__mbHeader : {
		*(.__mbHeader)
	}

	.text ALIGN (0x1000) :
	{
		*(.text)
	}

	.data ALIGN (0x1000) :
	{
		*(.data)
		*(.rodata)
	}

	.bss ALIGN (0x1000) :
	{
		*(.bss)
	}

	kernel_end = .;
}
