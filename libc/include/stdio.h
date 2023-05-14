#ifndef _LIBC_STDIO_H_
#define _LIBC_STDIO_H_

#include <sys/types.h>
#include <stdarg.h>

#define NULL				((void *) 0)
#define UNUSED(x)			((void) (x))
#define ALIGN_DOWN(base, size)		((base) & -((__typeof__(base))(size)))
#define ALIGN_UP(base, size)		ALIGN_DOWN((base) + (size) - 1, (size))

#define BUFSIZ				1024
#define EOF				(-1)

typedef struct _FILE FILE;

/*
 * FILE struct.
 */
struct _FILE {
	int			fd;							/* file descriptor */
	int 			flags;							/* flags */
	off_t			offset;							/* offset */
	unsigned char *		buf;							/* buffer */
	size_t			buf_size;						/* buffer size */
	unsigned char *		rpos;							/* current read position in buffer */
	unsigned char *		rend;							/* end of read buffer */
	unsigned char *		wpos;							/* current write position in buffer */
	unsigned char *		wend;							/* end of write buffer */
	unsigned char *		wbase;							/* start of write buffer */
	int			lbf;							/* line end character */
	void *			cookie;							/* private cookie */
	size_t 			(*read)(FILE *, unsigned char *, size_t);		/* read operation */
	size_t 			(*write)(FILE *, const unsigned char *, size_t);	/* write operation */
	off_t 			(*seek)(FILE *, off_t, int);				/* seek operation */
	struct _FILE *		prev;							/* previous opened file */
	struct _FILE *		next;							/* next opened file */
};

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

FILE *fdopen(int fd, const char *mode);
FILE *fopen(const char *pathname, const char *mode);
int fclose(FILE *fp);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *fp);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *fp);
int fgetc(FILE *fp);
int getc(FILE *fp);
int getchar(void);
int fputc(int c, FILE *fp);
int putc(int c, FILE *fp);
int putchar(int c);
char *fgets(char *s, int size, FILE *fp);
int fseek(FILE *fp, long offset, int whence);
long ftell(FILE *fp);
void rewind(FILE *fp);
int fflush(FILE *fp);
int feof(FILE *fp);
int ferror(FILE *fp);
int fileno(FILE *fp);
int rename(const char *oldpath, const char *newpath);

ssize_t getdelim(char **lineptr, size_t *size, int delim, FILE *fp);
ssize_t getline(char **lineptr, size_t *n, FILE *fp);

int printf(const char *format, ...);
int fprintf(FILE *fp, const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);

int vprintf(const char *format, va_list ap);
int vfprintf(FILE *fp, const char *format, va_list ap);
int vsprintf(char *str, const char *format, va_list ap);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

void perror(const char *msg);

#endif
