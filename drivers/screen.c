#include <x86/io.h>
#include <drivers/screen.h>
#include <stddef.h>

static uint16_t *video_memory = (uint16_t *) VIDEO_MEMORY;
static uint8_t cursor_x = 0;
static uint8_t cursor_y = 0;

/*
 * Move the cursor.
 */
static void move_cursor()
{
  uint16_t cursor_offset = cursor_y * NB_COLS + cursor_x;

  outb(REG_SCREEN_CTRL, 14);
  outb(REG_SCREEN_DATA, cursor_offset >> 8);
  outb(REG_SCREEN_CTRL, 15);
  outb(REG_SCREEN_DATA, cursor_offset);
}

/*
 * Clear the screen.
 */
void screen_clear()
{
  int i;

  /* blank all screen */
  for (i = 0; i < NB_ROWS * NB_COLS; i++)
    video_memory[i] = ' ';

  /* reset cursor */
  cursor_x = 0;
  cursor_y = 0;
  move_cursor();
}

/*
 * Scroll the screen up by one line.
 */
static void screen_scroll()
{
  int i;

  if(cursor_y >= NB_ROWS) {
    /* copy lines to upper ones */
    for (i = 0; i < (NB_ROWS - 1) * NB_COLS; i++)
      video_memory[i] = video_memory[i + NB_COLS];

    /* blank last line */
    for (i = (NB_ROWS - 1) * NB_COLS; i < NB_ROWS * NB_COLS; i++)
      video_memory[i] = ' ';

    cursor_y = NB_ROWS - 1;
  }
}

/*
 * Write a character to the screen.
 */
void screen_putc(char c)
{
  uint8_t *location;

  /* handle special characters */
  if (c == BACKSPACE_KEY && cursor_x) {
    cursor_x--;
  } else if (c == TAB_KEY) {
    cursor_x = (cursor_x + TAB_SIZE) & ~(TAB_SIZE - 1);
  } else if (c == '\r') {
    cursor_x = 0;
  } else if (c == '\n') {
    cursor_x = 0;
    cursor_y++;
  } else if(c >= ' ') {
    location = (uint8_t *) (video_memory + (cursor_y * NB_COLS + cursor_x));
    location[0] = c;
    location[1] = WHITE_ON_BLACK;
    cursor_x++;
  }

  /* insert a new line */
  if (cursor_x >= NB_COLS) {
    cursor_x = 0;
    cursor_y++;
  }

  /* scroll if needed */
  screen_scroll();

  /* update cursor */
  move_cursor();
}
