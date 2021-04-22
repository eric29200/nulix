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
    return ENOEXEC;

	if (elf_header->e_ident[EI_CLASS] != ELFCLASS32)
    return ENOEXEC;

	if (elf_header->e_ident[EI_DATA] != ELFDATA2LSB)
    return ENOEXEC;

	if (elf_header->e_ident[EI_VERSION] != EV_CURRENT)
    return ENOEXEC;

	if (elf_header->e_machine != EM_386)
    return ENOEXEC;

	if (elf_header->e_type != ET_EXEC)
    return ENOEXEC;

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
  uint32_t vaddr;
  char *buf = NULL;
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

  /* map binary pages to memory */
  ph = (struct elf_prog_header_t *) (buf + elf_header->e_phoff);
  for (vaddr = PAGE_ALIGN(ph->p_vaddr); vaddr <= ph->p_vaddr + ph->p_filesz; vaddr += PAGE_SIZE)
    alloc_frame(get_page(vaddr, 1, current_task->pgd), 0, 0);

  /* copy binary */
  memcpy((void *) ph->p_vaddr, buf + ph->p_offset, ph->p_filesz);

  /* set elf entry point */
  elf_layout->entry = (void *) elf_header->e_entry;

out:
  if (buf)
    kfree(buf);
  return elf_layout;
}
