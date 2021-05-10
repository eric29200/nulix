global loader                               	; the entry symbol for ELF
global kernel_stack			    	; export kernel stack
extern kmain                                	; the entry symbol for C

MAGIC_NUMBER  equ 0x1BADB002                	; define multi boot constants
PAGE_ALIGN    equ 1<<0
MEM_INFO      equ 1<<1
FLAGS         equ PAGE_ALIGN | MEM_INFO
CHECKSUM      equ -(MAGIC_NUMBER + FLAGS)
KSTACK_SIZE   equ 0x100000                    	; size of kernel stack = 1 MB

section .__mbHeader                         	; multiboot header
align 4
	dd MAGIC_NUMBER
	dd FLAGS
	dd CHECKSUM

section .bss					; kernel stack
align 4
kernel_stack:
	resb KSTACK_SIZE

section .text
align 4
loader:                                     	; the loader label (defined as entry point in linker script)
setup_kstack:
	mov esp, kernel_stack + KSTACK_SIZE     ; point esp to the start of the stack (end of memory area)

call_kmain:
	push esp				; push kernel stack address
	push ebx                                ; push multiboot header pointer
	push eax                                ; push multiboot magic
	call kmain                              ; call main C function

.loop:
	hlt					; loop forever
	jmp .loop

