#ifndef _LIBC_STDIO_IMPL_H_
#define _LIBC_STDIO_IMPL_H_

#include <stdio.h>

#define UNGET			8

#define F_PERM			1
#define F_NORD			4
#define F_NOWR			8
#define F_EOF			16
#define F_ERR			32
#define F_SVB			64
#define F_APP			128

#define MIN(x, y)		((x) < (y) ? (x) : (y))
#define MAX(x, y)		((x) > (y) ? (x) : (y))

extern FILE *files_list;

void __stdio_init();
void __stdio_exit();
size_t __stdio_read(FILE *fp, unsigned char *buf, size_t len);
size_t __stdio_write(FILE *fp, const unsigned char *buf, size_t len);
off_t __stdio_seek(FILE *fp, off_t off, int whence);

int __fmodeflags(const char *mode);
size_t __fwritex(const unsigned char *ptr, size_t len, FILE *fp);

int __to_read(FILE *fp);
int __to_write(FILE *fp);

FILE *__file_add(FILE *fp);
void __file_del(FILE *fp);

#endif