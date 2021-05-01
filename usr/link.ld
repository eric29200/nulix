OUTPUT_FORMAT(elf32-i386)
ENTRY(main)

SECTIONS {
	. = 0xA00000;

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
}
