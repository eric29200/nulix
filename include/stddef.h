#ifndef _STDDEF_H_
#define _STDDEF_H_

typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned char uint8_t;
typedef char int8_t;

typedef unsigned int size_t;
typedef unsigned int off_t;
typedef unsigned char bool;
typedef unsigned int time_t;
typedef int pid_t;

#define TRUE  1
#define FALSE 0

#define NULL        ((void *) 0)
#define UNUSED(x)   ((void) (x))

#define offsetof(type, member)	              ((size_t) & ((type *) 0)->member)
#define container_of(ptr, type, member)       ({                                                \
                                               void *__mptr = (void *)(ptr);					          \
	                                             ((type *)(__mptr - offsetof(type, member))); })

#endif
