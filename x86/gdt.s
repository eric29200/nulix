global gdt_flush

gdt_flush:
	mov eax, [esp+4]	; load the gdt pointer passed as parameter on the stack
	lgdt [eax]

	mov ax, 0x10		; load data segement offset
	mov ds, ax		; load data segment selectors
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	jmp 0x08:.flush		; jump to code segment
.flush:
	ret
