#ifndef _LIBC_DEV_H_
#define _LIBC_DEV_H_

#define major(x)	((unsigned)( (((x) >> 31 >> 1) & 0xFFFFF000) | (((x) >> 8) & 0x00000FFF) ))
#define minor(x)	((unsigned)( (((x) >> 12) & 0xFFFFFF00) | ((x) & 0x000000FF) ))

#define makedev(x,y) (				\
        (((x) & 0xFFFFF000ULL) << 32)		\
	| (((x) & 0x00000FFFULL) << 8)		\
        | (((y) & 0xFFFFFF00ULL) << 12)		\
	| (((y) & 0x000000FFULL)))

#endif
