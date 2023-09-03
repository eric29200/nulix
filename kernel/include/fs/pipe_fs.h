#ifndef _PIPE_FS_H_
#define _PIPE_FS_H_

#include <fs/fs.h>

#define PIPE_WPOS(inode)	((inode)->i_zone[0])
#define PIPE_RPOS(inode)	((inode)->i_zone[1])
#define PIPE_SIZE(inode)	((PIPE_WPOS(inode) - PIPE_RPOS(inode)) & (PAGE_SIZE - 1))
#define PIPE_EMPTY(inode)	(PIPE_WPOS(inode) == PIPE_RPOS(inode))
#define PIPE_FULL(inode)	(PIPE_SIZE(inode) == (PAGE_SIZE - 1))

/*
 * Pipe inode.
 */
struct pipe_inode_info {
	uint32_t	i_zone[2];
};

#endif
