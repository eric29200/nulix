#ifndef _LIBC_CTYPE_H_
#define _LIBC_CTYPE_H_

static inline int isspace(int c)
{
	return c == ' ' || (unsigned) c - '\t' < 5;
}

static inline int isdigit(int c)
{
	return (unsigned) c - '0' < 10;
}

#endif