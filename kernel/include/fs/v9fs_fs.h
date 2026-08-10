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
	uint32_t		maxdata;
};

/* 9p super operations */
int init_v9fs_fs();

#endif