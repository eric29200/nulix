#ifndef _LIBC_DIRENT_H_
#define _LIBC_DIRENT_H_

#include <stdio.h>
#include <stdint.h>

typedef struct {
	off_t		tell;
	int		fd;
	int		buf_pos;
	int		buf_end;
	char		buf[2048];
} DIR;

struct dirent {
	ino_t		d_ino;
	off_t		d_off;
	unsigned short	d_reclen;
	unsigned char	d_type;
	char		d_name[256];
};

DIR *opendir(const char *name);
int closedir(DIR *dirp);
struct dirent *readdir(DIR *dirp);

#endif
