#ifndef _ZLIB_H_
#define _ZLIB_H_

#include <stddef.h>

#define Z_OK		0
#define Z_ERROR		1

/*
 * Zlib stream.
 */
struct zlib_stream {
	uint8_t *	in;			/* input buffer */
	size_t		in_len;			/* input buffer length */
	size_t		in_pos;			/* position in input buffer */
	uint8_t		in_bit_pos;		/* position in current byte in input buffer */
	uint8_t *	out;			/* output buffer */
	size_t		out_len;		/* output buffer length */
	size_t		out_pos;		/* position in output buffer */
	size_t		out_written;		/* number of bytes written to output buffer */
};

int zlib_inflate_reset(struct zlib_stream *stream);
int zlib_inflate(struct zlib_stream *stream);

#endif