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

static inline int islower(int c)
{
	return (unsigned) c - 'a' < 26;
}

static inline int isupper(int c)
{
	return (unsigned) c - 'A' < 26;
}

static inline int tolower(int c)
{
	return isupper(c) ? c | 32 : c;
}

static inline int toupper(int c)
{
	return islower(c) ? c & 0x5F : c;
}

#endif