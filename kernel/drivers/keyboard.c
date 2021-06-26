#include <drivers/keyboard.h>
#include <x86/interrupt.h>
#include <x86/io.h>
#include <drivers/tty.h>
#include <ipc/signal.h>
#include <dev.h>

#define KEYBOARD_PORT               0x60
#define KEYBOARD_ACK                0x61
#define KEYBOARD_STATUS             0x64
#define KEYBOARD_LED_CODE           0xED

#define KEYBOARD_STATUS_SHIFT       0x01
#define KEYBOARD_STATUS_CTRL        0x02
#define KEYBOARD_STATUS_ALT         0x04
#define KEYBOARD_STATUS_CAPSLOCK    0x08

/* keyboard status */
static uint32_t keyboard_status = 0;

/* normal key map */
static unsigned char key_map[] = {
    0,   27,  '&',  233,  '"', '\'',  '(',  '-',
  232,  '_',  231,  224,  ')',  '=',  127,    9,
  'a',  'z',  'e',  'r',  't',  'y',  'u',  'i',
  'o',  'p',  '^',  '$',   13,  0,  'q',  's',
  'd',  'f',  'g',  'h',  'j',  'k',  'l',  'm',
  249,  178,    0,   42,  'w',  'x',  'c',  'v',
  'b',  'n',  ',',  ';',  ':',  '!',    0,  '*',
    0,   32,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  '-',    0,    0,    0,  '+',    0,
    0,    0,    0,    0,    0,    0,  '<',    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0 };

/* shift key map */
static unsigned char shift_map[] = {
    0,   27,  '1',  '2',  '3',  '4',  '5',  '6',
  '7',  '8',  '9',  '0',  176,  '+',  127,    9,
  'A',  'Z',  'E',  'R',  'T',  'Y',  'U',  'I',
  'O',  'P',  168,  163,   13,    0,  'Q',  'S',
  'D',  'F',  'G',  'H',  'J',  'K',  'L',  'M',
  '%',    0,    0,  181,  'W',  'X',  'C',  'V',
  'B',  'N',  '?',  '.',  '/',  167,    0,  '*',
    0,   32,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  '-',    0,    0,    0,  '+',    0,
    0,    0,    0,    0,    0,    0,  '>',    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0 };

/* alt key map */
static unsigned char alt_map[] = {
    0,    0,    0,  '~',  '#',  '{',  '[',  '|',
  '`', '\\',   '^',  '@', ']',  '}',    0,    0,
  '@',    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  164,   13,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  '|',    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0 };

/* escape map */
static unsigned char esc_map[] = {
  0,    0,    0,    0,    0,    0,    0,    0,
  0,    0,    0,    0,    0,    0,    0,    0,
  0,    0,    0,    0,    0,    0,    0,    0,
  0,    0,    0,    0,    0,    0,    0,    0,
  0,    0,    0,    0,    0,    0,    0,    0,
  0,    0,    0,    0,    0,    0,    0,    0,
  0,    0,    0,    0,    0,    0,    0,    0,
  0,    0,    0,    0,    0,    0,    0,    0,
  0,    0,    0,    0,    0,    0,    0,    KEY_HOME,
  KEY_UP,    KEY_PAGEUP,    0,    KEY_LEFT,    0,    KEY_RIGHT,    0,    KEY_END,
  KEY_DOWN,    KEY_PAGEDOWN,    KEY_INSERT,    KEY_DELETE,    0,    0,    0,    0,
  0,    0,    0,    0,    0,    0,    0,    0,
  0 };

/*
 * Scan keyboard key.
 */
static uint32_t scan_key()
{
  uint32_t code, value;

  /* get code and value */
  code = inb(KEYBOARD_PORT);
  value = inb(KEYBOARD_ACK);

  /* ack keyboard value */
  outb(KEYBOARD_ACK, value | 0x80);
  outb(KEYBOARD_ACK, value);

  return code;
}

/*
 * Keyboard handler.
 */
static void keyboard_handler(struct registers_t *regs)
{
  uint32_t scan_code, key_code;

  UNUSED(regs);

  /* get key */
  scan_code = scan_key();

  switch (scan_code) {
    case 0x2A:
    case 0x36:
      keyboard_status |= KEYBOARD_STATUS_SHIFT;
      break;
    case 0xAA:
    case 0xB6:
      keyboard_status &= ~KEYBOARD_STATUS_SHIFT;
      break;
    case 0x1D:
      keyboard_status |= KEYBOARD_STATUS_CTRL;
      break;
    case 0x9D:
      keyboard_status &= ~KEYBOARD_STATUS_CTRL;
      break;
    case 0x38:
      keyboard_status |= KEYBOARD_STATUS_ALT;
      break;
    case 0xB8:
      keyboard_status &= ~KEYBOARD_STATUS_ALT;
      break;
    case 0x3A:
      keyboard_status ^= KEYBOARD_STATUS_CAPSLOCK;
      break;
    default:
      /* on key down, send char to current tty */
      if ((scan_code & 0x80) == 0) {
        if ((keyboard_status & KEYBOARD_STATUS_CTRL) != 0) {
          key_code = key_map[scan_code];

          /* SIGINT/SIGSTOP signals */
          if (key_code == 'c')
            tty_signal_group(DEV_TTY0, SIGINT);
          else if (key_code == 'z')
            tty_signal_group(DEV_TTY0, SIGSTOP);

          key_code = '\n';
        } else if ((keyboard_status & KEYBOARD_STATUS_ALT) != 0) {
          /* F keys : change tty */
          if (scan_code >= KEY_F1 && scan_code <= KEY_F10) {
            tty_change(scan_code - KEY_F1);
            return;
          }

          key_code = alt_map[scan_code];
        } else if ((((keyboard_status & KEYBOARD_STATUS_SHIFT) != 0)
                    ^ ((keyboard_status & KEYBOARD_STATUS_CAPSLOCK) != 0)) != 0) {
          key_code = shift_map[scan_code];
        } else {
          key_code = key_map[scan_code];
        }

        /* check escape map */
        if (!key_code)
          key_code = esc_map[scan_code];

        /* send character to tty */
        if (key_code)
          tty_update(key_code);
      }
      break;
  }
}

/*
 * Init keyboard.
 */
void init_keyboard()
{
  register_interrupt_handler(33, keyboard_handler);
}
