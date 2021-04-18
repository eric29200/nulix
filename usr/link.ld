OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

SECTIONS {
	. = 0x002000000;

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
