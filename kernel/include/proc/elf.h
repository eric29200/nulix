#ifndef _ELF_H_
#define _ELF_H_

#include <stddef.h>

/* e_ident */
#define EI_MAG0       0
#define EI_MAG1       1
#define EI_MAG2       2
#define EI_MAG3       3
#define EI_CLASS      4
#define EI_DATA       5
#define EI_VERSION    6
#define EI_OSABI      7
#define EI_ABIVERSION 8
#define EI_PAD        9
#define EI_NIDENT     16

/* magic number */
#define ELFMAG0       0x7F
#define ELFMAG1       'E'
#define ELFMAG2       'L'
#define ELFMAG3       'F'

/* class */
#define ELFCLASSNONE  0	    /* invalid class */
#define ELFCLASS32    1	    /* 32-bit objects */
#define ELFCLASS64    2	    /* 64-bit objects */

/* byte order */
#define ELFDATANONE   0     /* invalid data encoding */
#define ELFDATA2LSB   1     /* little endian */
#define ELFDATA2MSB   2     /* big endian */

/* version */
#define EV_NONE       0	    /* invalid version */
#define EV_CURRENT    1     /* current version */

/* type */
#define ET_NONE       0     /* unkown type */
#define ET_REL        1     /* relocatable file */
#define ET_EXEC       2     /* executable file */
#define ET_DYN        3     /* share object file */
#define ET_CORE       4     /* core file */

/* machine */
#define EM_NONE       0	    /* no machine */
#define EM_386        3	    /* intel 80386 */

/*
 * ELF header.
 */
struct elf_header_t {
    uint8_t e_ident[EI_NIDENT];   /* magic number */
    uint16_t e_type;              /* object file type */
    uint16_t e_machine;           /* architecture */
    uint32_t e_version;           /* object file version */
    uint32_t e_entry;             /* entry point virtual address */
    uint32_t e_phoff;             /* program header table file offset */
    uint32_t e_shoff;             /* section header table file offset */
    uint32_t e_flags;             /* processor-specific flags */
    uint16_t e_ehsize;            /* ELF header size in bytes */
    uint16_t e_phentsize;         /* program header table entry size */
    uint16_t e_phnum;             /* program header table entry count */
    uint16_t e_shentsize;         /* section header table entry size */
    uint16_t e_shnum;             /* section header table entry count */
    uint16_t e_shstrndx;          /* section header strin table index */
};

/*
 * ELF program header.
 */
struct elf_prog_header_t {
    uint32_t p_type;              /* segment type */
    uint32_t p_offset;            /* segment file offset */
    uint32_t p_vaddr;             /* segment virtual address */
    uint32_t p_paddr;             /* segment physical address */
    uint32_t p_filesz;            /* segment size in file */
    uint32_t p_memsz;             /* segment size in memory */
    uint32_t p_flags;             /* segment flags */
    uint32_t p_align;             /* segment alignment */
};

/*
 * ELF layout.
 */
struct elf_layout_t {
  uint32_t stack;
  uint32_t entry;
};

struct elf_layout_t *elf_load(const char *path);

#endif
