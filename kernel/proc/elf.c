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
struct elf_layout_t *elf_load(const char *path)
{
  struct elf_layout_t *elf_layout = NULL;
  struct elf_header_t *elf_header;
  struct elf_prog_header_t *ph;
  struct stat_t statbuf;
  char *buf = NULL;
  uint32_t i;
  int fd;

  /* get file size */
  if (sys_stat((char *) path, &statbuf) < 0)
    return NULL;

  /* allocate buffer */
  buf = (char *) kmalloc(statbuf.st_size);
  if (!buf)
    goto out;

  /* open file */
  fd = sys_open(path);
  if (fd < 0)
    goto out;

  /* load file in memory */
  if ((uint32_t) sys_read(fd, buf, statbuf.st_size) != statbuf.st_size) {
    sys_close(fd);
    goto out;
  }

  /* close file */
  sys_close(fd);

  /* check elf header */
  elf_header = (struct elf_header_t *) buf;
  if (elf_check(elf_header) != 0)
    goto out;

  /* create elf layout */
  elf_layout = (struct elf_layout_t *) kmalloc(sizeof(struct elf_layout_t));
  if (!elf_layout)
    goto out;

  /* map binary pages to user memory */
  ph = (struct elf_prog_header_t *) (buf + elf_header->e_phoff);
  for (i = 0; i < PAGE_ALIGN_UP(ph->p_filesz) / PAGE_SIZE; i++)
    alloc_frame(get_page(UMEM_START + i * PAGE_SIZE, 1, current_task->pgd), 0, 0);

  /* copy binary */
  memcpy((void *) UMEM_START, buf + ph->p_offset, ph->p_filesz);

  /* allocate a stack just after the binary */
  elf_layout->stack = PAGE_ALIGN_UP(UMEM_START + ph->p_filesz);
  for (i = 0; i < USTACK_SIZE / PAGE_SIZE; i++)
    alloc_frame(get_page(elf_layout->stack + i * PAGE_SIZE, 1, current_task->pgd), 0, 1);

  /* set elf entry point */
  elf_layout->entry = UMEM_START + (elf_header->e_entry - ph->p_vaddr);
  elf_layout->stack += USTACK_SIZE;
out:
  if (buf)
    kfree(buf);
  return elf_layout;
}
