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
ISR 0 ; ISR 1 ; ISR 2 ; ISR 3 ; ISR 4 ; ISR 5 ; ISR 6 ; ISR 7 ; ISR_ERR 8 ; ISR 9 ; ISR_ERR 10 ; ISR_ERR 11 ; ISR_ERR 12
ISR_ERR 13 ; ISR_ERR 14 ; ISR 15 ; ISR 16 ; ISR 17 ; ISR 18 ; ISR 19 ; ISR 20 ; ISR 21 ; ISR 22 ; ISR 23 ; ISR 24 ; ISR 25
ISR 26 ; ISR 27 ; ISR 28 ; ISR 29 ; ISR 30 ; ISR 31 ; ISR 128

/* generate irqs */
IRQ 0, 32 ; IRQ 1, 33 ; IRQ 2, 34 ; IRQ 3, 35 ; IRQ 4, 36 ; IRQ 5, 37 ; IRQ 6, 38 ; IRQ 7, 39 ; IRQ 8, 40 ; IRQ 9, 41
IRQ 10, 42 ; IRQ 11, 43 ; IRQ 12, 44 ; IRQ 13, 45 ; IRQ 14, 46 ; IRQ 15, 47

/* common isr handler */
isr_common:
	pusha				/* save registers */
	push %ds
	push %es
	push %fs
	push %gs

	mov $KERNEL_DSEG, %ax		/* load kernel data segment descriptor */
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs

	push %esp			/* push a pointer to this frame */
	call isr_handler		/* call C generic isr */
	add $4, %esp

	pop %gs				/* restore registers */
	pop %fs
	pop %es
	pop %ds
	popa
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

	pushl 72(%eax) 			/* user data segment */
	pushl 68(%eax)	 		/* push our current stack */
	pushl 64(%eax)			/* EFLAGS */
	pushl 60(%eax) 			/* segment selector */
	pushl 56(%eax) 			/* eip */

	mov 16(%eax), %edi
	mov 20(%eax), %esi
	mov 24(%eax), %ebp
	mov 32(%eax), %ebx
	mov 36(%eax), %edx
	mov 40(%eax), %ecx
	mov 44(%eax), %eax

	iret
