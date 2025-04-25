#include <x86/segment.h>

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
ISR 32
ISR 33
ISR 34
ISR 35
ISR 36
ISR 37
ISR 38
ISR 39
ISR 40
ISR 41
ISR 42
ISR 43
ISR 44
ISR 45
ISR 46
ISR 47
ISR 128

.extern isr_handler

/* common isr handler */
isr_common:
	pusha			/* save registers */

	mov %ds, %ax
	push %eax

	mov $KERNEL_DS, %ax	/* load kernel data segment descriptor */
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs

	push %esp		/* push a pointer to is frame */
	call isr_handler	/* call C generic isr */
	add $4, %esp

	pop %ebx
	mov %bx, %ds
	mov %bx, %es
	mov %bx, %fs
	mov %bx, %gs

	popa			/* restore registers */
	add $8, %esp
	iret
