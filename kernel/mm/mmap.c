#include <mm/mmap.h>
#include <mm/mm.h>
#include <proc/sched.h>

/*
 * Memory map system call.
 */
void *do_mmap(void *addr, size_t length, int prot, int flags)
{
  struct list_head_t *pos;
  struct vm_area_t *vm;
  uint32_t max_addr = 0;

  /* find max mapped address */
  list_for_each(pos, &current_task->vm_list) {
    vm = list_entry(pos, struct vm_area_t, list);
    if (vm->vm_end > max_addr)
      max_addr = vm->vm_end;
  }

  /* no memory regions : start */
  if (!max_addr)
    max_addr = UMAP_START;

  /* create a new memory region */
  vm = (struct vm_area_t *) kmalloc(sizeof(struct vm_area_t));
  if (!vm)
    return NULL;

  /* set memory region */
  vm->vm_start = max_addr;
  vm->vm_end = max_addr + length;
  vm->vm_flags = flags;
  INIT_LIST_HEAD(&vm->list);
  list_add(&vm->list, &current_task->vm_list);

  return (void *) vm->vm_start;
}

/*
 * Memory unmap system call.
 */
int do_munmap(void *addr, size_t length)
{
  return 0;
}
