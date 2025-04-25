.global gdt_flush
.global tss_flush
.global idt_flush

gdt_flush:
	mov 4(%esp), %eax	/* load the gdt pointer passed as parameter on the stack */
	lgdt (%eax)

	mov $0x10, %ax		/* load data segement offset */
	mov %ax, %ds		/* load data segment selectors */
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	mov %ax, %ss
	ljmp $0x08, $.flush	/* jump to code segment */
.flush:
	ret

tss_flush:
	 mov $0x2B, %ax		/* load the tss structure */
	 ltr %ax
	 ret

idt_flush:
	mov 4(%esp), %eax	/* load the idt pointer passed as parameter on stack */
	lidt (%eax)
	ret
