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