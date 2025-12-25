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
	popa					;\
	add $8, %esp

#define LOAD_KERNEL_DSEG			\
	mov $KERNEL_DSEG, %ax			;\
	mov %ax, %ds				;\
	mov %ax, %es				;\
	mov %ax, %fs				;\
	mov %ax, %gs

#define EXCEPTION				\
	push %esp				;\
	call exception_handler			;\
	add $4, %esp

#define IRQ					\
	push %esp				;\
	call irq_handler			;\
	add $4, %esp

#define BUILD_EXCEPTION(num, name)		\
.globl name; name:				;\
	cli					;\
	push $0					;\
	push $num				;\
	SAVE_ALL				;\
	LOAD_KERNEL_DSEG			;\
	EXCEPTION				;\
	RESTORE_ALL				;\
	iret

#define BUILD_EXCEPTION_ERR(num, name)		\
.globl name; name:				;\
	cli					;\
	push $num				;\
	SAVE_ALL				;\
	LOAD_KERNEL_DSEG			;\
	EXCEPTION				;\
	RESTORE_ALL				;\
	iret

#define BUILD_IRQ(num, name)			\
.globl name; name:				;\
	cli					;\
	push $0					;\
	push $num				;\
	SAVE_ALL				;\
	LOAD_KERNEL_DSEG			;\
	IRQ					;\
	RESTORE_ALL				;\
	iret

/* build exceptions */
BUILD_EXCEPTION(0, exception0)
BUILD_EXCEPTION(1, exception1)
BUILD_EXCEPTION(2, exception2)
BUILD_EXCEPTION(3, exception3)
BUILD_EXCEPTION(4, exception4)
BUILD_EXCEPTION(5, exception5)
BUILD_EXCEPTION(6, exception6)
BUILD_EXCEPTION(7, exception7)
BUILD_EXCEPTION_ERR(8, exception8)
BUILD_EXCEPTION(9, exception9)
BUILD_EXCEPTION_ERR(10, exception10)
BUILD_EXCEPTION_ERR(11, exception11)
BUILD_EXCEPTION_ERR(12, exception12)
BUILD_EXCEPTION_ERR(13, exception13)
BUILD_EXCEPTION_ERR(14, exception14)
BUILD_EXCEPTION(15, exception15)
BUILD_EXCEPTION(16, exception16)
BUILD_EXCEPTION(17, exception17)
BUILD_EXCEPTION(18, exception18)
BUILD_EXCEPTION(19, exception19)
BUILD_EXCEPTION(20, exception20)
BUILD_EXCEPTION(21, exception21)
BUILD_EXCEPTION(22, exception22)
BUILD_EXCEPTION(23, exception23)
BUILD_EXCEPTION(24, exception24)
BUILD_EXCEPTION(25, exception25)
BUILD_EXCEPTION(26, exception26)
BUILD_EXCEPTION(27, exception27)
BUILD_EXCEPTION(28, exception28)
BUILD_EXCEPTION(29, exception29)
BUILD_EXCEPTION(30, exception30)
BUILD_EXCEPTION(31, exception31)
BUILD_EXCEPTION(128, exception128)

/* buid irqs */
BUILD_IRQ(0, irq0)
BUILD_IRQ(1, irq1)
BUILD_IRQ(2, irq2)
BUILD_IRQ(3, irq3)
BUILD_IRQ(4, irq4)
BUILD_IRQ(5, irq5)
BUILD_IRQ(6, irq6)
BUILD_IRQ(7, irq7)
BUILD_IRQ(8, irq8)
BUILD_IRQ(9, irq9)
BUILD_IRQ(10, irq10)
BUILD_IRQ(11, irq11)
BUILD_IRQ(12, irq12)
BUILD_IRQ(13, irq13)
BUILD_IRQ(14, irq14)
BUILD_IRQ(15, irq15)

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
