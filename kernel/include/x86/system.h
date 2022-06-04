#ifndef _SYSTEM_H_
#define _SYSTEM_H_

#include <stddef.h>

#define KERNEL_CODE_SEGMENT	0x08
#define KERNEL_DATA_SEGMENT	0x10
#define USER_CODE_SEGMENT	0x18
#define USER_DATA_SEGMENT	0x20
#define TSS_SEGMENT		0x28

struct registers_t {
	uint32_t ds;
	uint32_t edi;
	uint32_t esi;
	uint32_t ebp;
	uint32_t esp;
	uint32_t ebx;
	uint32_t edx;
	uint32_t ecx;
	uint32_t eax;
	uint32_t int_no;
	uint32_t err_code;
	uint32_t eip;
	uint32_t cs;
	uint32_t eflags;
	uint32_t useresp;
	uint32_t ss;
};

/*
 * Halt the processor.
 */
static inline void halt()
{
	__asm__ __volatile__("hlt");
}

#endif
