global loader
global kernel_stack
extern kmain

KSTACK_SIZE	 equ 0x100000			; size of kernel stack = 1 MB

section .multiboot				; multiboot2 section
align 0x1000
mb_header_start:
	dd 0xe85250d6
	dd 0
	dd mb_header_end - mb_header_start
	dd -(0xe85250d6 + 0 + (mb_header_end - mb_header_start))

align 8
mb_fb_start:
	dw 5
	dw 1
	dd mb_fb_end - mb_fb_start
	dd 0
	dd 0
	dd 0
mb_fb_end:

align 8
mb_tag_end:
	dw 0
	dw 0
	dd 8
mb_header_end:

section .bss					; kernel stack
align 0x1000
kernel_stack:
	resb KSTACK_SIZE

section .text
align 0x1000
loader:
	mov esp, kernel_stack + KSTACK_SIZE	; point esp to the start of the stack (end of memory area)

call_kmain:
	push esp				; push kernel stack address
	push ebx				; push multiboot header pointer
	push eax				; push multiboot magic
	call kmain				; call main C function

.loop:
	hlt					; loop forever
	jmp .loop

