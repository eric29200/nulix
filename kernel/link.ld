ENTRY(loader)

SECTIONS {
	. = 0x00100000;
	kernel_start = .;

	.text ALIGN (0x1000) :
	{
		*(.multiboot)
		*(.text)
	}

	.rodata ALIGN (0x1000) :
	{
		*(.rodata)
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

	kernel_end = .;
}
