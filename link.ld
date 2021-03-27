OUTPUT_FORMAT(elf32-i386)
ENTRY(loader)

SECTIONS {
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
	}

	.bss ALIGN (0x1000) :
	{
		*(COMMON)
		*(.bss)
	}

	kernel_end = .; _kernel_end = .; __kernel_end = .;
}
