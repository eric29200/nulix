#ifndef _SYS_TYPES_H_
#define _SYS_TYPES_H_

#define NULL      ((void *) 0)

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
typedef uint64_t ino_t;
typedef unsigned uid_t;
typedef unsigned gid_t;

#endif
