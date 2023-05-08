#ifndef _LIBC_LIMITS_H_
#define _LIBC_LIMITS_H_

#define CHAR_BIT	8
#define SCHAR_MIN	(-128)
#define SCHAR_MAX	127
#define UCHAR_MAX	255
#define SHRT_MIN	(-1 - 0x7FFF)
#define SHRT_MAX	0x7fff
#define USHRT_MAX	0xffff
#define INT_MIN		(-1 - 0x7FFFFFFF)
#define INT_MAX		0x7FFFFFFF
#define UINT_MAX	0xFFFFFFFFU
#define LONG_MIN	(-LONG_MAX - 1)
#define LONG_MAX	__LONG_MAX
#define ULONG_MAX	(2UL * LONG_MAX + 1)
#define LLONG_MIN	(-LLONG_MAX - 1)
#define LLONG_MAX	0x7FFFFFFFFFFFFFFFLL
#define ULLONG_MAX	(2ULL * LLONG_MAX + 1)

#endif