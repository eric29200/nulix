#ifndef _SCREEN_H_
#define _SCREEN_H_

#include <stddef.h>

#define SCREEN_MEM                0xB8000

#define SCREEN_WIDTH              80
#define SCREEN_HEIGHT             25

#define SCREEN_BLACK              0
#define SCREEN_LIGHT_GREY         7
#define MAKE_COLOR(bg, fg)        (((bg) << 4) | (fg))
#define MAKE_ENTRY(bg, fg, c)     ((MAKE_COLOR((bg), (fg)) << 8) | (c))

/*
 * Screen struct.
 */
struct screen_t {
  uint32_t  pos_x;
  uint32_t  pos_y;
  uint8_t   dirty;
  char      buf[SCREEN_WIDTH * SCREEN_HEIGHT];
};

void screen_init(struct screen_t *screen);
void screen_write(struct screen_t *screen, const char *buf, size_t n);
void screen_putc(struct screen_t *screen, char c);
void screen_update(struct screen_t *screen);

#endif
