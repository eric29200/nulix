; nasm macro to create a isr function with no error code
%macro ISR_NOERRCODE 1
	global isr%1
	isr%1:
		cli
		push 0
		push %1
		jmp isr_common_stub
%endmacro

; nasm macro to create a isr function with error code
%macro ISR_ERRCODE 1
	global isr%1
	isr%1:
		cli
		push %1
		jmp isr_common_stub
%endmacro

; nasm macro to create a irq function with 2 parameters (interrupt and irq numbers)
%macro IRQ 2
	global irq%1
	irq%1:
		cli
		push 0
		push %2
		jmp isr_common_stub
%endmacro

; generate CPU-dedicated isr (from 0 to 31)
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE 8
ISR_NOERRCODE 9
ISR_ERRCODE 10
ISR_ERRCODE 11
ISR_ERRCODE 12
ISR_ERRCODE 13
ISR_ERRCODE 14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_NOERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31
ISR_NOERRCODE 128

; generate irqs
IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

extern isr_handler

; common isr handler
isr_common_stub:
	pusha			; save registers

	mov ax, ds
	push eax

	mov ax, 0x10		; load kernel data segment descriptor
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	push esp		; push a pointer to is frame
	call isr_handler	; call C generic isr
	add esp, 4

	pop ebx
	mov ds, bx
	mov es, bx
	mov fs, bx
	mov gs, bx

	popa			; restore registers
	add esp, 8
	iret
