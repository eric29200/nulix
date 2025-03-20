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
#include <nano-X.h>

#include "doomgeneric.h"
#include "doomkeys.h"

#define KEY_QUEUE_SIZE			16

/* nano-x context */
static GR_GC_ID gc;
static GR_WINDOW_ID window;
static uint8_t *window_buf = NULL;
static const int win_size_x = DOOMGENERIC_RESX;
static const int win_size_y = DOOMGENERIC_RESY;

/* key queue */
static uint16_t key_queue[KEY_QUEUE_SIZE];
static uint8_t key_queue_widx = 0;
static uint8_t key_queue_ridx = 0;

/*
 * Init function.
 */
void DG_Init()
{
	/* try to open a connection */
	if (GrOpen() < 0) {
		GrError("GrOpen failed");
		exit(1);
	}

	/* create context */
	gc = GrNewGC();
	GrSetGCUseBackground(gc, GR_FALSE);
	GrSetGCForeground(gc, MWRGB(255, 0, 0));

	/* create new window */
	window = GrNewBufferedWindow(GR_WM_PROPS_APPFRAME
		| GR_WM_PROPS_CAPTION 
		| GR_WM_PROPS_CLOSEBOX
		| GR_WM_PROPS_BUFFER_MMAP
		| GR_WM_PROPS_BUFFER_BGRA,
		"Doom", GR_ROOT_WINDOW_ID, 50, 50,
		win_size_x, win_size_y,
		MWRGB(255, 255, 255));

	/* select events */
	GrSelectEvents(window, GR_EVENT_MASK_EXPOSURE
		| GR_EVENT_MASK_TIMER
		| GR_EVENT_MASK_CLOSE_REQ
		| GR_EVENT_MASK_BUTTON_DOWN
		| GR_EVENT_MASK_BUTTON_UP
		| GR_EVENT_MASK_KEY_DOWN
		| GR_EVENT_MASK_KEY_UP);

	/* map window */
	GrMapWindow(window);

	/* get frame buffer */
	window_buf = GrOpenClientFramebuffer(window);
}

/*
 * Handle keyboard event.
 */
static void __handle_keyboard_event(GR_EVENT *event, int pressed)
{
	uint16_t key_data;
	uint8_t key;

	/* convert to doom key */
	switch (event->keystroke.ch) {
		case MWKEY_ENTER:
			key = KEY_ENTER;
			break;
		case MWKEY_ESCAPE:
			key = KEY_ESCAPE;
			break;
		case MWKEY_LEFT:
			key = KEY_LEFTARROW;
			break;
		case MWKEY_RIGHT:
			key = KEY_RIGHTARROW;
			break;
		case MWKEY_UP:
			key = KEY_UPARROW;
			break;
		case MWKEY_DOWN:
			key = KEY_DOWNARROW;
			break;
		case ' ':
			key = KEY_USE;
			break;
		case MWKEY_RCTRL:
		case MWKEY_LCTRL:
			key = KEY_FIRE;
			break;
		case MWKEY_RSHIFT:
		case MWKEY_LSHIFT:
			key = KEY_RSHIFT;
			break;
		case 'y':
		case 'Y':
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
	GR_EVENT event;

	/* handle events */
	while (GrPeekEvent(&event)) {
		/* get next event */
		GrGetNextEvent(&event);

		/* handle event */
		switch (event.type) {
			case GR_EVENT_TYPE_KEY_DOWN:
				__handle_keyboard_event(&event, 1);
				break;
			case GR_EVENT_TYPE_KEY_UP:
				__handle_keyboard_event(&event, 0);
				break;
			case GR_EVENT_TYPE_CLOSE_REQ:
				GrClose();
				exit(0);
				break;
			case GR_EVENT_TYPE_EXPOSURE:
				break;
			case GR_EVENT_TYPE_TIMER:         
				break;
		}
	}

	/* update framebuffer */
	memcpy(window_buf, DG_ScreenBuffer, DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
        GrFlushWindow(window);
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
	GrSetWindowTitle(window, title);
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