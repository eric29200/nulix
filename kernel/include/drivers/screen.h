#ifndef _SCREEN_H_
#define _SCREEN_H_

#include <stddef.h>

#define SCREEN_WIDTH      80
#define SCREEN_HEIGHT     25

/*
 * Screen struct.
 */
struct screen_t {
  uint32_t pos_x;
  uint32_t pos_y;
  uint8_t dirty;
  char buf[SCREEN_WIDTH * SCREEN_HEIGHT];
};

void screen_init(struct screen_t *screen);
void screen_write(struct screen_t *screen, const char *buf, size_t n);
void screen_putc(struct screen_t *screen, char c);
void screen_update(struct screen_t *screen);

#endif
