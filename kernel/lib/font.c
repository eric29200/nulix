#include <font.h>

#define NR_FONTS	(sizeof(fonts) / sizeof(*fonts))

/*
 * Fonts table.
 */
static struct font_desc *fonts[] = {
	&font_lat9_8x8,
	&font_lat9_8x10,
	&font_lat9_8x12,
	&font_lat9_8x14,
	&font_lat9_8x16,
};

/*
 * Find a font.
 */
struct font_desc *font_find(size_t width, size_t height)
{
	size_t i;

	for (i = 0; i < NR_FONTS; i++)
		if (fonts[i]->width == width && fonts[i]->height == height)
			return fonts[i];

	return NULL;
}
