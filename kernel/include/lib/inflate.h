#ifndef _INFLATE_H_
#define _INFLATE_H_

#include <stddef.h>

#define INFLATE_OK		0
#define INFLATE_ERROR		1

int inflate(uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len);

#endif