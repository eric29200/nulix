#include <proc/elf.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <fs/fs.h>
#include <fcntl.h>
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
  uint32_t i, j;

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
  fd = do_open(path, O_RDONLY, 0);
  if (fd < 0) {
    ret = fd;
    goto out;
  }

  /* load file in memory */
  if ((size_t) do_read(fd, buf, statbuf.st_size) != statbuf.st_size) {
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

  /* unmap current text pages */
  for (i = current_task->start_text; i <= current_task->end_text; i += PAGE_SIZE)
    unmap_page(i, current_task->pgd);

  /* set start text segment */
  ph = (struct elf_prog_header_t *) (buf + elf_header->e_phoff);
  current_task->start_text = ph->p_vaddr;

  /* copy elf in memory */
  for (i = 0; i < elf_header->e_phnum; i++, ph++) {
    /* skip non relocable segments */
    if (ph->p_type != ET_REL)
      continue;

    /* map pages */
    for (j = ph->p_vaddr; j < PAGE_ALIGN_UP(ph->p_vaddr + ph->p_filesz); j += PAGE_SIZE)
      map_page(j, current_task->pgd, 0, ph->p_flags & FLAG_WRITE ? 1 : 0);

    /* copy in memory */
    memset((void *) ph->p_vaddr, 0, ph->p_memsz);
    memcpy((void *) ph->p_vaddr, buf + ph->p_offset, ph->p_filesz);

    /* update stack position */
    current_task->end_text = ph->p_vaddr + ph->p_filesz;
  }

  /* set data segment */
  current_task->start_brk = PAGE_ALIGN_UP(current_task->end_text + PAGE_SIZE);
  current_task->end_brk = PAGE_ALIGN_UP(current_task->end_text + PAGE_SIZE);

  /* allocate at the end of process memory */
  current_task->user_stack = USTACK_START - USTACK_SIZE;
  for (i = 0; i <= USTACK_SIZE / PAGE_SIZE; i++)
    map_page(current_task->user_stack + i * PAGE_SIZE, current_task->pgd, 0, 1);

  /* set elf entry point */
  current_task->user_entry = elf_header->e_entry;
  current_task->user_stack = USTACK_START;
out:
  if (buf)
    kfree(buf);
  return ret;
}
