#ifndef _PIPE_I_H_
#define _PIPE_I_H_

#include <stddef.h>

#define PIPE_WPOS(inode)		((inode)->u.pipe_i.i_rpos)
#define PIPE_RPOS(inode)		((inode)->u.pipe_i.i_wpos)
#define PIPE_SIZE(inode)		((PIPE_WPOS(inode) - PIPE_RPOS(inode)) & (PAGE_SIZE - 1))
#define PIPE_EMPTY(inode)		(PIPE_WPOS(inode) == PIPE_RPOS(inode))
#define PIPE_FULL(inode)		(PIPE_SIZE(inode) == (PAGE_SIZE - 1))

/*
 * Pipefs in memory inode.
 */
struct pipe_inode_info_t {
	uint32_t	i_rpos;
	uint32_t	i_wpos;
	char		i_rwait;
	char		i_wwait;
};

#endif
