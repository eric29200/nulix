global loader
global kernel_stack
extern kmain

KSTACK_SIZE   equ 0x100000                    	; size of kernel stack = 1 MB

section .multiboot				; multiboot2 section
align 4
header_start:
	dd 0xe85250d6
	dd 0
	dd header_end - header_start
	dd -(0xe85250d6 + 0 + (header_end - header_start))
	dw 0
	dw 0
	dd 8
header_end:

section .bss					; kernel stack
align 4
kernel_stack:
	resb KSTACK_SIZE

section .text
align 4
loader:
	mov esp, kernel_stack + KSTACK_SIZE     ; point esp to the start of the stack (end of memory area)

call_kmain:
	push esp				; push kernel stack address
	push ebx                                ; push multiboot header pointer
	push eax                                ; push multiboot magic
	call kmain                              ; call main C function

.loop:
	hlt					; loop forever
	jmp .loop

