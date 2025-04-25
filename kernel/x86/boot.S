.global loader
.global kernel_stack
.extern kmain

#define KSTACK_SIZE	 	0x100000	/* size of kernel stack = 1 MB */
#define MB_HEADER_MAGIC		0xe85250d6
#define MB_HEADER_FLAGS		0

.section .multiboot				/* multiboot2 section */
.align 0x1000
mb_header_start:
	.long MB_HEADER_MAGIC
	.long MB_HEADER_FLAGS
	.long mb_header_end - mb_header_start
	.long -(MB_HEADER_MAGIC + MB_HEADER_FLAGS + (mb_header_end - mb_header_start))

.align 8
mb_fb_start:
	.word 5
	.word 1
	.long mb_fb_end - mb_fb_start
	.long 0
	.long 0
	.long 0
mb_fb_end:

.align 8
mb_tag_end:
	.word 0
	.word 0
	.long 8
mb_header_end:

.section .bss					/* kernel stack */
.align 0x1000
kernel_stack_ptr:
	.skip KSTACK_SIZE
kernel_stack:

.section .text
.align 0x1000
loader:
	mov $kernel_stack, %esp			/* point esp to the start of the stack (end of memory area) */

call_kmain:
	pushl %ebx				/* push multiboot header pointer */
	pushl %eax				/* push multiboot magic */
	call kmain				/* call main C function */

.loop:
	hlt					/* loop forever */
	jmp .loop

