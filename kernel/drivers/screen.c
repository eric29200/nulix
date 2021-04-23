#include <drivers/screen.h>
#include <string.h>

/*
 * Write a character on screen.
 */
void screen_putc(struct screen_t *screen, char c)
{
  int i;

  /* handle character */
  if (c >= ' ' && c <= '~') {
    screen->buf[screen->pos_y * SCREEN_WIDTH + screen->pos_x] = c;
    screen->pos_x++;
  } else if (c == '\b' && screen->pos_x != 0) {
    screen->pos_x--;
  } else if (c == '\t') {
    screen->pos_x = (screen->pos_x + 4) & ~0x03;
  } else if (c == '\n') {
    screen->pos_y++;
    screen->pos_x = 0;
  } else if (c == '\r') {
    screen->pos_x = 0;
  }

  /* go to next line */
  if (screen->pos_x >= SCREEN_WIDTH) {
    screen->pos_x = 0;
    screen->pos_y++;
  }

  /* scroll */
  if (screen->pos_y >= SCREEN_HEIGHT) {
    /* move each line up */
    for (i = 0; i < SCREEN_WIDTH * (SCREEN_HEIGHT - 1); i++)
      screen->buf[i] = screen->buf[i + SCREEN_WIDTH];

    /* clear last line */
    memset(&screen->buf[i], ' ', SCREEN_WIDTH * SCREEN_HEIGHT);

    screen->pos_y = SCREEN_HEIGHT - 1;
  }

  /* mark screen dirty */
  screen->dirty = 1;
}

/*
 * Write a string to the screen.
 */
void screen_write(struct screen_t *screen, const char *buf, size_t n)
{
  size_t i;

  for (i = 0; i < n; i++)
    screen_putc(screen, buf[i]);
}

/*
 * Init the screen.
 */
void screen_init(struct screen_t *screen)
{
  memset(screen->buf, 0, sizeof(screen->buf));
  screen->pos_x = 0;
  screen->pos_y = 0;
  screen->dirty = 1;
}
