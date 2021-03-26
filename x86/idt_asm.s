[GLOBAL idt_flush]
idt_flush:
	mov eax, [esp+4]	; load the idt pointer passed as parameter on stack
	lidt [eax]
	ret
