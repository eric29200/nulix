#ifndef _STDDEF_H_
#define _STDDEF_H_

typedef unsigned long long uint64_t;
typedef long long int64_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned char uint8_t;
typedef char int8_t;

typedef uint32_t size_t;
typedef int32_t ssize_t;
typedef int64_t off_t;
typedef int64_t time_t;
typedef int32_t pid_t;
typedef unsigned mode_t;
typedef uint64_t dev_t;
typedef uint64_t ino_t;
typedef unsigned uid_t;
typedef unsigned gid_t;

#define NULL                                  ((void *) 0)
#define UNUSED(x)                             ((void) (x))

#define offsetof(type, member)                ((size_t) & ((type *) 0)->member)
#define container_of(ptr, type, member)       ({void *__mptr = (void *)(ptr);                   \
                                              ((type *)(__mptr - offsetof(type, member))); })

#define div_floor(x, y)                       ((x) / (y))
#define div_ceil(x, y)                        (((x) / (y)) + (((x) % (y)) > 0 ? 1 : 0))

#define major(dev)                            ((unsigned short) ((dev) >> 8))
#define minor(dev)                            ((unsigned short) ((dev) & 0xFF))
#define mkdev(major, minor)                   (((major) << 8) | (minor))

#endif
