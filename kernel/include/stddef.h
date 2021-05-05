#ifndef _STDDEF_H_
#define _STDDEF_H_

typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned char uint8_t;
typedef char int8_t;

typedef unsigned int size_t;
typedef int off_t;
typedef unsigned int time_t;
typedef int pid_t;
typedef unsigned short mode_t;
typedef unsigned short dev_t;
typedef unsigned short ino_t;
typedef unsigned short uid_t;
typedef unsigned short gid_t;

#define NULL                                  ((void *) 0)
#define UNUSED(x)                             ((void) (x))

#define offsetof(type, member)	              ((size_t) & ((type *) 0)->member)
#define container_of(ptr, type, member)       ({void *__mptr = (void *)(ptr);					          \
	                                             ((type *)(__mptr - offsetof(type, member))); })

#define div_floor(x, y)                       ((x) / (y))
#define div_ceil(x, y)                        (((x) / (y)) + (((x) % (y)) > 0 ? 1 : 0))

#define major(dev)                            ((unsigned short) ((dev) >> 8))
#define minor(dev)                            ((unsigned short) ((dev) & 0xFF))
#define mkdev(major, minor)                   (((major) << 8) | (minor))

#endif
