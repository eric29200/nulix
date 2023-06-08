#ifndef _MMAP_H_
#define _MMAP_H_

#include <fs/fs.h>
#include <lib/list.h>
#include <stddef.h>

#define VM_READ			0x01
#define VM_WRITE		0x02
#define VM_EXEC			0x04
#define VM_SHARED		0x08
#define VM_GROWSDOWN		0x0100
#define VM_GROWSUP		0x0200
#define VM_SHM			0x0400
#define VM_DENYWRITE		0x0800
#define VM_EXECUTABLE		0x1000
#define VM_LOCKED		0x2000

#define MAP_SHARED       	1
#define MAP_PRIVATE      	2
#define MAP_TYPE         	0xF
#define MAP_FIXED        	0x10
#define MAP_ANONYMOUS    	0x20

#define MREMAP_MAYMOVE		1
#define MREMAP_FIXED		2

#define PROT_READ		0x1		/* page can be read */
#define PROT_WRITE		0x2		/* page can be written */
#define PROT_EXEC		0x4		/* page can be executed */
#define PROT_NONE		0x0		/* page can not be accessed */

void *do_mmap(uint32_t addr, size_t length, int prot, int flags, struct file_t *filp, off_t offset);
int do_munmap(uint32_t addr, size_t length);
void *do_mremap(uint32_t old_address, size_t old_size, size_t new_size, int flags, uint32_t new_address);
struct vm_area_t *find_vma(struct task_t *task, uint32_t addr);
struct vm_area_t *find_vma_prev(struct task_t *task, uint32_t addr);
struct vm_area_t *find_vma_next(struct task_t *task, uint32_t addr);
struct vm_area_t *find_vma_intersection(struct task_t *task, uint32_t start, uint32_t end);
void vmtruncate(struct inode_t *inode, off_t offset);

void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
void *sys_mmap2(void *addr, size_t length, int prot, int flags, int fd, off_t pgoffset);
int sys_munmap(void *addr, size_t length);
void *sys_mremap(void *old_address, size_t old_size, size_t new_size, int flags, void *new_address);
int sys_madvise(void *addr, size_t length, int advice);
int sys_mprotect(void *addr, size_t len, int prot);
uint32_t sys_brk(uint32_t brk);

#endif
