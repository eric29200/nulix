#include <x86/segment.h>

.extern exception_handler
.extern irq_handler

#define SAVE_ALL				\
	pusha					;\
	push %ds				;\
	push %es				;\
	push %fs				;\
	push %gs

#define RESTORE_ALL				\
	pop %gs					;\
	pop %fs					;\
	pop %es					;\
	pop %ds					;\
	popa

#define LOAD_KERNEL_DSEG			\
	mov $KERNEL_DSEG, %ax			;\
	mov %ax, %ds				;\
	mov %ax, %es				;\
	mov %ax, %fs				;\
	mov %ax, %gs

/* macro to create an exception function with no error code */
.macro BUILD_EXCEPTION num
	.globl exception\num
	exception\num:
		cli
		push $0
		push $\num
		SAVE_ALL
		LOAD_KERNEL_DSEG
		push %esp			/* push a pointer to this frame */
		call exception_handler		/* call C handler */
		add $4, %esp
		RESTORE_ALL
		add $8, %esp
		iret
.endm


/* macro to create a exception function with error code */
.macro BUILD_EXCEPTION_ERR num
	.globl exception\num
	exception\num:
		cli
		push $\num
		SAVE_ALL
		LOAD_KERNEL_DSEG
		push %esp			/* push a pointer to this frame */
		call exception_handler		/* call C handler */
		add $4, %esp
		RESTORE_ALL
		add $8, %esp
		iret
.endm

/* macro to create a irq function */
.macro BUILD_IRQ num
	.globl irq\num
	irq\num:
		cli
		push $0
		push $\num
		SAVE_ALL
		LOAD_KERNEL_DSEG
		push %esp			/* push a pointer to this frame */
		call irq_handler		/* call C handler */
		add $4, %esp
		RESTORE_ALL
		add $8, %esp
		iret
.endm

/* build exceptions */
BUILD_EXCEPTION 0
BUILD_EXCEPTION 1
BUILD_EXCEPTION 2
BUILD_EXCEPTION 3
BUILD_EXCEPTION 4
BUILD_EXCEPTION 5
BUILD_EXCEPTION 6
BUILD_EXCEPTION 7
BUILD_EXCEPTION_ERR 8
BUILD_EXCEPTION 9
BUILD_EXCEPTION_ERR 10
BUILD_EXCEPTION_ERR 11
BUILD_EXCEPTION_ERR 12
BUILD_EXCEPTION_ERR 13
BUILD_EXCEPTION_ERR 14
BUILD_EXCEPTION 15
BUILD_EXCEPTION 16
BUILD_EXCEPTION 17
BUILD_EXCEPTION 18
BUILD_EXCEPTION 19
BUILD_EXCEPTION 20
BUILD_EXCEPTION 21
BUILD_EXCEPTION 22
BUILD_EXCEPTION 23
BUILD_EXCEPTION 24
BUILD_EXCEPTION 25
BUILD_EXCEPTION 26
BUILD_EXCEPTION 27
BUILD_EXCEPTION 28
BUILD_EXCEPTION 29
BUILD_EXCEPTION 30
BUILD_EXCEPTION 31
BUILD_EXCEPTION 128

/* buid irqs */
BUILD_IRQ 0
BUILD_IRQ 1
BUILD_IRQ 2
BUILD_IRQ 3
BUILD_IRQ 4
BUILD_IRQ 5
BUILD_IRQ 6
BUILD_IRQ 7
BUILD_IRQ 8
BUILD_IRQ 9
BUILD_IRQ 10
BUILD_IRQ 11
BUILD_IRQ 12
BUILD_IRQ 13
BUILD_IRQ 14
BUILD_IRQ 15

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
