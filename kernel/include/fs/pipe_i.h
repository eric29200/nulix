#ifndef _PIPE_I_H_
#define _PIPE_I_H_

#include <stddef.h>

#define PIPE_BUF			PAGE_SIZE

#define PIPE_BASE(inode)		((inode)->u.pipe_i.i_base)
#define PIPE_START(inode)		((inode)->u.pipe_i.i_start)
#define PIPE_LEN(inode)			((inode)->u.pipe_i.i_len)
#define PIPE_WAIT(inode)		((inode)->u.pipe_i.i_wait)
#define PIPE_READERS(inode)		((inode)->u.pipe_i.i_readers)
#define PIPE_WRITERS(inode)		((inode)->u.pipe_i.i_writers)
#define PIPE_RD_OPENERS(inode)		((inode)->u.pipe_i.i_rd_openers)
#define PIPE_WR_OPENERS(inode)		((inode)->u.pipe_i.i_wr_openers)

#define PIPE_SIZE(inode)		(PIPE_LEN(inode))
#define PIPE_EMPTY(inode)		(PIPE_SIZE(inode) == 0)
#define PIPE_FULL(inode)		(PIPE_SIZE(inode) == PIPE_BUF)
#define PIPE_FREE(inode)		(PIPE_BUF - PIPE_LEN(inode))
#define PIPE_END(inode)			((PIPE_START(inode) + PIPE_LEN(inode)) & (PIPE_BUF - 1))
#define PIPE_MAX_RCHUNK(inode)		(PIPE_BUF - PIPE_START(inode))
#define PIPE_MAX_WCHUNK(inode)		(PIPE_BUF - PIPE_END(inode))

/*
 * Pipefs in memory inode.
 */
struct pipe_inode_info {
	char *			i_base;
	int			i_start;
	int			i_len;
	size_t			i_readers;
	size_t			i_writers;
	size_t			i_rd_openers;
	size_t			i_wr_openers;
	struct wait_queue *	i_wait;
};

#endif
