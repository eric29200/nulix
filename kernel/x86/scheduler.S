#include <x86/segment.h>

.global scheduler_do_switch
.global enter_user_mode
.global return_user_mode

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
