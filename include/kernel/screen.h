#ifndef _SCREEN_H_
#define _SCREEN_H_

#define VIDEO_MEMORY      0xB8000
#define NB_ROWS           25
#define NB_COLS           80
#define WHITE_ON_BLACK    0x0F

#define REG_SCREEN_CTRL   0x3D4
#define REG_SCREEN_DATA   0x3D5

#define BACKSPACE_KEY     0x08
#define TAB_KEY           0x09
#define TAB_SIZE          8

void screen_clear();
void screen_putc(char c);
void screen_puts(const char *s);
void screen_puti(int);

#endif
