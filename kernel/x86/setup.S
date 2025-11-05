#include <x86/segment.h>

.extern exception_handler
.extern irq_handler

/* macro to create an exception function with no error code */
.macro EXCEPTION num
    .globl exception\num
    exception\num:
        cli
        push $0
        push $\num
        jmp exception_common
.endm


/* nasm macro to create a exception function with error code */
.macro EXCEPTION_ERR num
    .globl exception\num
    exception\num:
        cli
        push $\num
        jmp exception_common
.endm

/* nasm macro to create a irq function */
.macro IRQ num
	.globl irq\num
	irq\num:
		cli
        	push $0
		push $\num
		jmp irq_common
.endm

/* generate exceptions */
EXCEPTION 0 ; EXCEPTION 1 ; EXCEPTION 2 ; EXCEPTION 3 ; EXCEPTION 4 ; EXCEPTION 5 ; EXCEPTION 6 ;
EXCEPTION 7 ; EXCEPTION_ERR 8 ; EXCEPTION 9 ; EXCEPTION_ERR 10 ; EXCEPTION_ERR 11 ; EXCEPTION_ERR 12 ;
EXCEPTION_ERR 13 ; EXCEPTION_ERR 14 ; EXCEPTION 15 ; EXCEPTION 16 ; EXCEPTION 17 ; EXCEPTION 18 ;
EXCEPTION 19 ; EXCEPTION 20 ; EXCEPTION 21 ; EXCEPTION 22 ; EXCEPTION 23 ; EXCEPTION 24 ; EXCEPTION 25 ;
EXCEPTION 26 ; EXCEPTION 27 ; EXCEPTION 28 ; EXCEPTION 29 ; EXCEPTION 30 ; EXCEPTION 31 ; EXCEPTION 128

/* generate irqs */
IRQ 0 ; IRQ 1 ; IRQ 2 ; IRQ 3 ; IRQ 4 ; IRQ 5 ; IRQ 6 ; IRQ 7 ; IRQ 8 ; IRQ 9
IRQ 10 ; IRQ 11 ; IRQ 12 ; IRQ 13 ; IRQ 14 ; IRQ 15

/* common exception handler */
exception_common:
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
	call exception_handler		/* call C handler */
	add $4, %esp

	pop %gs				/* restore registers */
	pop %fs
	pop %es
	pop %ds
	popa
	add $8, %esp
	iret

/* common irq handler */
irq_common:
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
	call irq_handler		/* call C handler */
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
