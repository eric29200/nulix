#ifndef _V9FS_FS_H_
#define _V9FS_FS_H_

#include <stddef.h>

#define V9FS_PORT		564
#define V9FS_DEFUSER		"nobody"
#define V9FS_DEFANAME		""

/*
 * 9p session.
 */
struct v9fs_session_info {
	char *			uname;
	char *			aname;
	struct p9_client *	client;
};

/*
 * File system entity information (server side).
 */
struct p9_qid {
	uint8_t			type;
	uint32_t		version;
	uint64_t		path;
};

/*
 * File system entity handle (client side).
 */
struct p9_fid {
	struct p9_client *	client;
	uint32_t		fid;
	int			mode;
	struct p9_qid		qid;
	uint32_t		iounit;
	uid_t			uid;
	void *			rdir;
	struct list_head	flist;
	struct list_head	dlist;
};

/* 9p super operations */
int init_v9fs_fs();

#endif