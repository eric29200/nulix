#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <linux/kd.h>

#include "doomgeneric.h"
#include "doomkeys.h"

#define KEY_QUEUE_SIZE			16


/* direct frame buffer */
static int fb_fd = -1;
static int *fb_buf = NULL;
static size_t fb_buf_size;
static struct fb_var_screeninfo fb_var;

/* console attributes */
static struct termios old_term;
static int old_kbd_mode = -1;

/* key queue */
static uint16_t key_queue[KEY_QUEUE_SIZE];
static uint8_t key_queue_widx = 0;
static uint8_t key_queue_ridx = 0;

/*
 * Init framebuffer.
 */
static int __init_framebuffer()
{
	/* open direct frame buffer */
	fb_fd = open("/dev/fb0", 0);
	if (fb_fd < 0) {
		fprintf(stderr, "Error: can't open direct framebuffer\n");
		return -1;
	}

	/* get screen size */
	if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &fb_var)) {
		fprintf(stderr, "Error: can't get screen information\n");
		close(fb_fd);
		return -1;
	}

	/* map frame buffer */
	fb_buf_size = fb_var.xres * fb_var.yres * 4;
	fb_buf = mmap(NULL, fb_buf_size, PROT_READ | PROT_WRITE, 0, fb_fd, 0);
	if (!fb_buf) {
		fprintf(stderr, "Error: can't map framebuffer\n");
		close(fb_fd);
		return -1;
	}

	return 0;
}

/*
 * Exit framebuffer.
 */
static void __exit_framebuffer()
{
	if (fb_buf)
		munmap(fb_buf, fb_buf_size);

	if (fb_fd >= 0)
		close(fb_fd);
}

/*
 * Init console.
 */
static int __init_console()
{
	struct termios new_term;

	/* get console attributes */
	if (tcgetattr(STDIN_FILENO, &old_term)) {
		fprintf(stderr, "Can't get console attributes\n");
		return -1;
	}

	/* set console attributes */
	new_term = old_term;
	new_term.c_iflag = 0;
	new_term.c_lflag &= ~(ECHO | ICANON | ISIG);
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_term)) {
		fprintf(stderr, "Can't set console attributes\n");
		return -1;
	}

	return 0;
}

/*
 * Restore initial console.
 */
static void __exit_console()
{
	/* restore console attributes */
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_term))
		fprintf(stderr, "Can't restore console attributes\n");
}

/*
 * Init keyboard.
 */
static int __init_keyboard()
{
	int flags;

	/* get keyboard mode */
	if (ioctl(STDIN_FILENO, KDGKBMODE, &old_kbd_mode)) {
		fprintf(stderr, "Error: can't get keyboard mode\n");
		return -1;
	}

	/* set keyboard mode */
	if (ioctl(STDIN_FILENO, KDSKBMODE, K_MEDIUMRAW)) {
		fprintf(stderr, "Error: Can't set keyboard mode\n");
		return -1;
	}

	/* set non blocking mode */
	flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

	return 0;
}

/*
 * Restore initial keyboard.
 */
static void __exit_keyboard()
{
	/* restore keyboard mode */
	if (ioctl(STDIN_FILENO, KDSKBMODE, old_kbd_mode))
		fprintf(stderr, "Error: Can't restore keyboard mode\n");
}

/*
 * Exit function.
 */
static void __exit()
{
	__exit_framebuffer();
	__exit_console();
	__exit_keyboard();
}

/*
 * Init function.
 */
void DG_Init()
{
	/* init framebuffer */
	if (__init_framebuffer())
		exit(1);

	/* init console */
	if (__init_console()) {
		__exit_framebuffer();
		exit(1);
	}

	/* init keyboard */
	if (__init_keyboard()) {
		__exit_console();
		exit(1);
	}

	/* restore at exit */
	atexit(__exit);
}

/*
 * Handle keyboard.
 */
static void __handle_keyboard()
{
	uint8_t scan_code, key_code, key;
	uint16_t key_data;
	int pressed;

	/* read from keyboard */
	if (read(STDIN_FILENO, &scan_code, 1) < 1)
		return;

	/* parse key */
	pressed = (scan_code & 0x80) == 0 ? 1 : 0;
	key_code = scan_code & 0x7F;

	/* convert to doom key */
	switch (key_code) {
		case 0x1C:
		case 0x60:
			key = KEY_ENTER;
			break;
		case 0x01:
			key = KEY_ESCAPE;
			break;
		case 0x69:
			key = KEY_LEFTARROW;
			break;
		case 0x6A:
			key = KEY_RIGHTARROW;
			break;
		case 0x67:
			key = KEY_UPARROW;
			break;
		case 0x6C:
			key = KEY_DOWNARROW;
			break;
		case 0x39:
			key = KEY_USE;
			break;
		case 0x1D:
		case 0x61:
			key = KEY_FIRE;
			break;
		case 0x2A:
		case 0x36:
			key = KEY_RSHIFT;
			break;
		case 0x15:
			key = 'y';
			break;
		default:
		 	key = 0;
			break;
	}

	/* queue key */
	key_data = (pressed << 8) | key;
	key_queue[key_queue_widx++] = key_data;
	key_queue_widx %= KEY_QUEUE_SIZE;
}

/*
 * Draw frame.
 */
void DG_DrawFrame()
{
	int i;

	/* draw framebuffer */
	if (fb_buf)
		for (i = 0; i < DOOMGENERIC_RESY; i++)
			memcpy(fb_buf + i * fb_var.xres, DG_ScreenBuffer + i * DOOMGENERIC_RESX, DOOMGENERIC_RESX * 4);

	/* handle keyboard */
	__handle_keyboard();
}

/*
 * Get key.
 */
int DG_GetKey(int *pressed, uint8_t *doom_key)
{
	uint16_t key_data;

	/* empty queue */
	if (key_queue_ridx == key_queue_widx)
		return 0;

	/* pop from queue */
	key_data = key_queue[key_queue_ridx++];
	key_queue_ridx %= KEY_QUEUE_SIZE;

	/* set result */
	*pressed = key_data >> 8;
	*doom_key = key_data & 0xFF;

	return 1;
}

/*
 * Sleep in ms.
 */
void DG_SleepMs(uint32_t ms)
{
	usleep(ms * 1000);
}

/*
 * Get time in ms.
 */
uint32_t DG_GetTicksMs()
{
	struct timezone tzp;
	struct timeval tp;

	gettimeofday(&tp, &tzp);

	return (tp.tv_sec * 1000) + (tp.tv_usec / 1000);
}

/*
 * Set window title.
 */
void DG_SetWindowTitle(const char *title)
{
}

/*
 * Main loop.
 */
int main(int argc, char **argv)
{
	/* init doom */
	doomgeneric_Create(argc, argv);

	/* main loop */
	for (;;)
		doomgeneric_Tick();

	return 0;
}