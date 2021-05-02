#include <proc/elf.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <fs/fs.h>
#include <stderr.h>
#include <string.h>

/*
 * Check header file
 */
static int elf_check(struct elf_header_t *elf_header)
{
	if (elf_header->e_ident[EI_MAG0] != ELFMAG0
      || elf_header->e_ident[EI_MAG1] != ELFMAG1
		  || elf_header->e_ident[EI_MAG2] != ELFMAG2
		  || elf_header->e_ident[EI_MAG3] != ELFMAG3)
    return -ENOEXEC;

	if (elf_header->e_ident[EI_CLASS] != ELFCLASS32)
    return -ENOEXEC;

	if (elf_header->e_ident[EI_DATA] != ELFDATA2LSB)
    return -ENOEXEC;

	if (elf_header->e_ident[EI_VERSION] != EV_CURRENT)
    return -ENOEXEC;

	if (elf_header->e_machine != EM_386)
    return -ENOEXEC;

	if (elf_header->e_type != ET_EXEC)
    return -ENOEXEC;

  return 0;
}

/*
 * Load an ELF file in memory.
 */
int elf_load(const char *path)
{
  struct elf_header_t *elf_header;
  struct elf_prog_header_t *ph;
  struct stat_t statbuf;
  char *buf = NULL;
  int fd, ret = 0;
  uint32_t i;

  /* get file size */
  ret = do_stat((char *) path, &statbuf);
  if (ret < 0)
    return ret;

  /* allocate buffer */
  buf = (char *) kmalloc(statbuf.st_size);
  if (!buf) {
    ret = -ENOMEM;
    goto out;
  }

  /* open file */
  fd = do_open(path);
  if (fd < 0) {
    ret = fd;
    goto out;
  }

  /* load file in memory */
  if ((uint32_t) do_read(fd, buf, statbuf.st_size) != statbuf.st_size) {
    ret = -EINVAL;
    do_close(fd);
    goto out;
  }

  /* close file */
  do_close(fd);

  /* check elf header */
  elf_header = (struct elf_header_t *) buf;
  if (elf_check(elf_header) != 0)
    goto out;

  /* set start text segment */
  ph = (struct elf_prog_header_t *) (buf + elf_header->e_phoff);
  current_task->start_text = ph->p_vaddr;

  /* copy elf in memory */
  for (; (char *) ph < (buf + elf_header->e_phoff + elf_header->e_phentsize * elf_header->e_phnum); ph++) {
    /* map page */
    alloc_frame(get_page(ph->p_vaddr, 1, current_task->pgd), 0, 1);

    /* copy in memory */
    memset((void *) ph->p_vaddr, 0, ph->p_memsz);
    memcpy((void *) ph->p_vaddr, buf + ph->p_offset, ph->p_filesz);

    /* update stack position */
    current_task->end_text = ph->p_vaddr + ph->p_filesz;
  }

  /* set data segment */
  current_task->start_brk = PAGE_ALIGN_UP(current_task->end_text);
  current_task->end_brk = PAGE_ALIGN_UP(current_task->end_text);

  /* allocate at the end of process memory */
  current_task->user_stack = USTACK_START - USTACK_SIZE;
  for (i = 0; i <= USTACK_SIZE / PAGE_SIZE; i++)
    alloc_frame(get_page(current_task->user_stack + i * PAGE_SIZE, 1, current_task->pgd), 0, 1);

  /* set elf entry point */
  current_task->user_entry = elf_header->e_entry;
  current_task->user_stack = USTACK_START;
out:
  if (buf)
    kfree(buf);
  return ret;
}
