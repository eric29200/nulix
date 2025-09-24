#ifndef _BINFMT_H_
#define _BINFMT_H_

/*
 * Binary format.
 */
struct binfmt {
	int (*load_binary)(const char *, struct binprm *);
	struct list_head	list;
};

int register_binfmt(struct binfmt *fmt);
void init_binfmt();
int init_elf_binfmt();
int init_script_binfmt();

#endif