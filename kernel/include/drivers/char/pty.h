#ifndef _PTY_H_
#define _PTY_H_

#define NR_PTYS			16

/*
 * Pty structure.
 */
struct pty {
	int			p_num;		/* pty id */
	int			p_count;	/* reference count */
};

int init_pty();

#endif
