#ifndef _ENDIAN_H_
#define _ENDIAN_H_

#include <stddef.h>

#define le16toh(x)	((uint16_t) x)
#define le32toh(x)	((uint32_t) x)

#define htole16(x)	((uint16_t) x)
#define htole32(x)	((uint32_t) x)

#endif
