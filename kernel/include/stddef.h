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
typedef uint32_t ino_t;
typedef unsigned uid_t;
typedef unsigned gid_t;
typedef int sigset_t;
typedef int clockid_t;

typedef unsigned char cc_t;
typedef unsigned long tcflag_t;

#define NFD_BITS				(8 * sizeof(uint32_t))
#define FDSET_SIZE				1024
#define FDSET_INTS				(FDSET_SIZE / NFD_BITS)

#define FD_CLR(d, s)				((s)->fds_bits[(d) / (8 * sizeof(long))] &= ~(1UL << ((d) % (8 * sizeof(long)))))
#define FD_SET(d, s)				((s)->fds_bits[(d) / (8 * sizeof(long))] |= (1UL << ((d) % (8 * sizeof(long)))))
#define FD_ISSET(d, s)				!!((s)->fds_bits[(d) / (8 * sizeof(long))] & (1UL << ((d) % (8 * sizeof(long)))))

typedef struct {
	uint32_t fds_bits[FDSET_SIZE / (8 * sizeof(uint32_t))];
} fd_set_t;

#define INT_MAX					((int) (~0U >> 1))
#define UINT_MAX				(~0U)
#define LONG_MAX				((long) (~0UL >> 1))
#define ULONG_MAX				(~0UL)

#define NULL					((void *) 0)
#define UNUSED(x)				((void) (x))

#define offsetof(type, member)			((size_t) & ((type *) 0)->member)
#define container_of(ptr, type, member)		({void *__mptr = (void *)(ptr);			\
						((type *)(__mptr - offsetof(type, member))); })

#define div_floor(x, y)				((x) / (y))
#define div_ceil(x, y)				(((x) / (y)) + (((x) % (y)) > 0 ? 1 : 0))

#define major(dev)				((dev) >> 8)
#define minor(dev)				((dev) & 0xFF)
#define mkdev(major, minor)		 	(((major) << 8) | (minor))

#endif
