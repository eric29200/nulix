#include <x86/segment.h>

.extern isr_handler

/* macro to create an isr function with no error code */
.macro ISR num
    .globl isr\num
    isr\num:
        cli
        push $0
        push $\num
        jmp isr_common
.endm


/* nasm macro to create a isr function with error code */
.macro ISR_ERR num
    .globl isr\num
    isr\num:
        cli
        push $\num
        jmp isr_common
.endm

/* nasm macro to create a irq function with 2 parameters (interrupt and irq numbers) */
.macro IRQ num ir
	.globl irq\num
	irq\num:
		cli
		push $0
		push $\ir
		jmp isr_common
.endm

/* generate isrs */
ISR 0
ISR 1
ISR 2
ISR 3
ISR 4
ISR 5
ISR 6
ISR 7
ISR_ERR 8
ISR 9
ISR_ERR 10
ISR_ERR 11
ISR_ERR 12
ISR_ERR 13
ISR_ERR 14
ISR 15
ISR 16
ISR 17
ISR 18
ISR 19
ISR 20
ISR 21
ISR 22
ISR 23
ISR 24
ISR 25
ISR 26
ISR 27
ISR 28
ISR 29
ISR 30
ISR 31
ISR 128

/* generate irqs */
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

/* common isr handler */
isr_common:
	pusha				/* save registers */

	mov %ds, %ax
	push %eax

	mov $KERNEL_DSEG, %ax		/* load kernel data segment descriptor */
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs

	push %esp			/* push a pointer to this frame */
	call isr_handler		/* call C generic isr */
	add $4, %esp

	pop %ebx
	mov %bx, %ds
	mov %bx, %es
	mov %bx, %fs
	mov $TLS_SEG, %bx
	mov %bx, %gs

	popa				/* restore registers */
	add $8, %esp
	iret

.global gdt_flush
gdt_flush:
	mov 4(%esp), %eax		/* load the gdt pointer passed as parameter on the stack */
	lgdt (%eax)

	mov $KERNEL_DSEG, %ax		/* load data segment offset */
	mov %ax, %ds			/* load data segment selectors */
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	mov %ax, %ss
	ljmp $KERNEL_CSEG, $.flush	/* jump to code segment */
.flush:
	ret

.global tss_flush
tss_flush:
	 mov 4(%esp), %ax		/* load the tss structure */
	 ltr %ax
	 ret

.global idt_flush
idt_flush:
	mov 4(%esp), %eax		/* load the idt pointer passed as parameter on stack */
	lidt (%eax)
	ret

.global copy_page_physical
copy_page_physical:
	push %ebx			/* save ebx and eflags */
	pushf

	mov 12(%esp), %ebx		/* src address */
	mov 16(%esp), %ecx		/* dst address */

	mov %cr0, %edx			/* disable paging */
	and $0x7fffffff, %edx
	mov %edx, %cr0

	mov $1024, %edx			/* 1024 * 4 bytes = 4096 bytes to copy */

.loop:
	mov (%ebx), %eax		/* get the word at the source address */
	mov %eax, (%ecx)		/* store it at the dest address */
	add $4, %ebx			/* src address += sizeof(word) */
	add $4, %ecx			/* dst address += sizeof(word) */
	dec %edx			/* one less word to do */
	jnz .loop

	mov %cr0, %edx			/* enable paging */
	or $0x80000000, %edx
	mov %edx, %cr0

	popf				/* restore eflags and ebx */
	pop %ebx
	ret

.global scheduler_do_switch
scheduler_do_switch:
	cli
	pusha

	mov 36(%esp), %eax		/* load current task's kernel stack address */
	mov %esp, (%eax)		/* save esp for current task's kernel stack */

	mov 40(%esp), %eax		/* load next task's kernel stack to esp */
	mov %eax, %esp

	popa
	sti
	ret

.global enter_user_mode
enter_user_mode:
	cli

	mov $USER_DSEG, %ax		/* set user segment registers */
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov $TLS_SEG, %ax
	mov %ax, %gs

	mov 4(%esp), %eax		/* get user stack */

	mov 12(%esp), %ebx		/* get user return address */
	movl %ebx, -4(%eax)

	mov 8(%esp), %ebx		/* get user eip */

	push $USER_DSEG			/* stack segment to restore */
	push %eax
	pushf

	pop %eax			/* enable interrupts */
	or $0x200, %eax
	push %eax

	push $USER_CSEG			/* code segment to restore */
	push %ebx
	iret

.global return_user_mode
return_user_mode:
	cli

	mov $USER_DSEG, %ax		/* set user segment registers */
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov $TLS_SEG, %ax
	mov %ax, %gs

	mov 4(%esp), %eax		/* get user stack */

	pushl 60(%eax) 			/* user data segment */
	pushl 56(%eax)	 		/* push our current stack */
	pushl 52(%eax)			/* EFLAGS */
	pushl 48(%eax) 			/* segment selector */
	pushl 44(%eax) 			/* eip */

	mov 4(%eax), %edi
	mov 8(%eax), %esi
	mov 12(%eax), %ebp
	mov 20(%eax), %ebx
	mov 24(%eax), %edx
	mov 28(%eax), %ecx
	mov 32(%eax), %eax

	iret