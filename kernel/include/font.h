#ifndef _FONT_H_
#define _FONT_H_

#include <stddef.h>

/*
 * Font description.
 */
struct font_desc {
	char *		name;
	size_t		width;
	size_t		height;
	uint8_t *	data;
};

extern struct font_desc font_lat9_8x8;
extern struct font_desc font_lat9_8x10;
extern struct font_desc font_lat9_8x12;
extern struct font_desc font_lat9_8x14;
extern struct font_desc font_lat9_8x16;

struct font_desc *font_find(size_t width, size_t height);

#endif
