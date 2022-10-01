#ifndef _ELF_H_
#define _ELF_H_

#include <proc/task.h>

/* e_ident */
#define EI_MAG0		0
#define EI_MAG1		1
#define EI_MAG2		2
#define EI_MAG3		3
#define EI_CLASS	4
#define EI_DATA		5
#define EI_VERSION	6
#define EI_OSABI	7
#define EI_ABIVERSION	8
#define EI_PAD		9
#define EI_NIDENT	16

/* magic number */
#define ELFMAG0		0x7F
#define ELFMAG1		'E'
#define ELFMAG2		'L'
#define ELFMAG3		'F'

/* class */
#define ELFCLASSNONE	0	/* invalid class */
#define ELFCLASS32	1	/* 32-bit objects */
#define ELFCLASS64	2	/* 64-bit objects */

/* byte order */
#define ELFDATANONE	0	/* invalid data encoding */
#define ELFDATA2LSB	1	/* little endian */
#define ELFDATA2MSB	2	/* big endian */

/* version */
#define EV_NONE		0	/* invalid version */
#define EV_CURRENT	1	/* current version */

/* type */
#define ET_NONE		0	/* unkown type */
#define ET_REL		1	/* relocatable file */
#define ET_EXEC		2	/* executable file */
#define ET_DYN		3	/* share object file */
#define ET_CORE		4	/* core file */

/* segment type */
#define PT_NULL		0
#define PT_LOAD		1
#define PT_DYNAMIC	2
#define PT_INTERP	3
#define PT_NOTE		4
#define PT_SHLIB	5
#define PT_PHDR		6
#define PT_NUM		7
#define PT_LOPROC	0x70000000
#define PT_HIPROC	0x7fffffff

/* machine */
#define EM_NONE		0	/* no machine */
#define EM_386		3	/* intel 80386 */

/* flags */
#define FLAG_EXEC	1	/* exec flag */
#define FLAG_WRITE	2	/* write flag */
#define FLAG_READ	4	/* read flag */

/* auxiliary table values */
#define AT_NULL   	0	/* end of vector */
#define AT_IGNORE 	1	/* entry should be ignored */
#define AT_EXECFD 	2	/* file descriptor of program */
#define AT_PHDR   	3	/* program headers for program */
#define AT_PHENT  	4	/* size of program header entry */
#define AT_PHNUM  	5	/* number of program headers */
#define AT_PAGESZ 	6	/* system page size */
#define AT_BASE   	7	/* base address of interpreter */
#define AT_FLAGS  	8	/* flags */
#define AT_ENTRY  	9	/* entry point of program */
#define AT_NOTELF 	10	/* program is not ELF */
#define AT_UID    	11	/* real uid */
#define AT_EUID   	12	/* effective uid */
#define AT_GID    	13	/* real gid */
#define AT_EGID   	14	/* effective gid */
#define AT_PLATFORM 	15  	/* string identifying CPU for optimizations */
#define AT_HWCAP  	16    	/* arch dependent hints at CPU capabilities */
#define AT_CLKTCK 	17	/* frequency at which times() increments */

/*
 * ELF header.
 */
struct elf_header_t {
		uint8_t		e_ident[EI_NIDENT];	/* magic number */
		uint16_t	e_type;			/* object file type */
		uint16_t	e_machine;		/* architecture */
		uint32_t	e_version;		/* object file version */
		uint32_t	e_entry;		/* entry point virtual address */
		uint32_t	e_phoff;		/* program header table file offset */
		uint32_t	e_shoff;		/* section header table file offset */
		uint32_t	e_flags;		/* processor-specific flags */
		uint16_t	e_ehsize;		/* ELF header size in bytes */
		uint16_t	e_phentsize;		/* program header table entry size */
		uint16_t	e_phnum;		/* program header table entry count */
		uint16_t	e_shentsize;		/* section header table entry size */
		uint16_t	e_shnum;		/* section header table entry count */
		uint16_t	e_shstrndx;		/* section header strin table index */
};

/*
 * ELF program header.
 */
struct elf_prog_header_t {
		uint32_t	p_type;			/* segment type */
		uint32_t	p_offset;		/* segment file offset */
		uint32_t	p_vaddr;		/* segment virtual address */
		uint32_t	p_paddr;		/* segment physical address */
		uint32_t	p_filesz;		/* segment size in file */
		uint32_t	p_memsz;		/* segment size in memory */
		uint32_t	p_flags;		/* segment flags */
		uint32_t	p_align;		/* segment alignment */
};

int elf_load(const char *path, struct binargs_t *bargs);

#endif
