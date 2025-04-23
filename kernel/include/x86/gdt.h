#ifndef _GDT_H_
#define _GDT_H_

#include <x86/descriptor.h>
#include <x86/segment.h>
#include <proc/task.h>

/* defined in x86/descriptor.s */
extern void gdt_flush(uint32_t);
extern void tss_flush();

void init_gdt();
void gdt_write_entry(size_t id, struct desc_struct *entry);
void load_tss(struct task *task);

#endif
