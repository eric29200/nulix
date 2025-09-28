#ifndef _MMAP_H_
#define _MMAP_H_

#include <fs/fs.h>
#include <lib/list.h>
#include <stddef.h>

/*
 * Old mmap argument.
 */
struct mmap_arg_struct {
	uint32_t		addr;
	uint32_t		len;
	uint32_t		prot;
	uint32_t		flags;
	uint32_t		fd;
	uint32_t		offset;
};

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
#define MAP_DENYWRITE		0x0800

#define MREMAP_MAYMOVE		1
#define MREMAP_FIXED		2

#define PROT_READ		0x1		/* page can be read */
#define PROT_WRITE		0x2		/* page can be written */
#define PROT_EXEC		0x4		/* page can be executed */
#define PROT_NONE		0x0		/* page can not be accessed */

#define MS_ASYNC		1
#define MS_INVALIDATE		2
#define MS_SYNC			4

void *do_mmap(struct file *filp, uint32_t addr, size_t length, int prot, int flags, off_t offset);
int do_munmap(uint32_t addr, size_t length);
struct vm_area *find_vma(struct task *task, uint32_t addr);
struct vm_area *find_vma_prev(struct task *task, uint32_t addr);
struct vm_area *find_vma_next(struct task *task, uint32_t addr);
struct vm_area *find_vma_intersection(struct task *task, uint32_t start, uint32_t end);
void vmtruncate(struct inode *inode, off_t offset);

int old_mmap(struct mmap_arg_struct *arg);
int sys_mmap2(uint32_t addr, size_t length, int prot, int flags, int fd, off_t pgoffset);
int sys_munmap(uint32_t addr, size_t length);
int sys_mremap(uint32_t old_address, size_t old_size, size_t new_size, int flags, uint32_t new_address);
int sys_madvise(uint32_t addr, size_t length, int advice);
int sys_mprotect(uint32_t addr, size_t len, int prot);
int sys_mincore(uint32_t addr, size_t len, unsigned char *vec);
uint32_t sys_brk(uint32_t brk);

#endif
