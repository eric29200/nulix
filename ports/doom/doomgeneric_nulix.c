#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>

#include "doomkeys.h"
#include "doomgeneric.h"

#define KEY_QUEUE_SIZE		16

/* key queue */
static unsigned short key_queue[KEY_QUEUE_SIZE];
static unsigned int key_queue_widx = 0;
static unsigned int key_queue_ridx = 0;

/* xlib stuff */
static Display *display = NULL;
static Window window = 0;
static int screen = 0;
static GC gc = 0;
static XImage *image = NULL;

/*
 * Convert to doom key.
 */
static unsigned char convert_to_doom_key(unsigned int key)
{
	switch (key)
	{
		case XK_Return:
			key = KEY_ENTER;
			break;
		case XK_Escape:
			key = KEY_ESCAPE;
			break;
		case XK_Left:
			key = KEY_LEFTARROW;
			break;
		case XK_Right:
			key = KEY_RIGHTARROW;
			break;
		case XK_Up:
			key = KEY_UPARROW;
			break;
		case XK_Down:
			key = KEY_DOWNARROW;
			break;
		case XK_Control_L:
		case XK_Control_R:
			key = KEY_FIRE;
			break;
		case XK_space:
			key = KEY_USE;
			break;
		case XK_Shift_L:
		case XK_Shift_R:
			key = KEY_RSHIFT;
			break;
		default:
			key = tolower(key);
			break;
	}

	return key;
}

/*
 * Add key to queue.
 */
static void add_key_to_queue(int pressed, unsigned int key_code)
{
	unsigned char key = convert_to_doom_key(key_code);
	unsigned short key_data = (pressed << 8) | key;

	key_queue[key_queue_widx++] = key_data;
	key_queue_widx %= KEY_QUEUE_SIZE;
}

/*
 * Init DOOM.
 */
void DG_Init()
{
	XSetWindowAttributes attr;
	int black, white, depth;
	XEvent e;

	/* clear key queue */
	memset(key_queue, 0, KEY_QUEUE_SIZE * sizeof(unsigned short));

	/* open display */
	display = XOpenDisplay(NULL);
	if (!display) {
		fprintf(stderr, "Can't open display\n");
		exit(1);
	}

	/* get default screen */
	screen = DefaultScreen(display);

	/* create window */
	memset(&attr, 0, sizeof(XSetWindowAttributes));
	attr.event_mask = ExposureMask | KeyPressMask;
	attr.background_pixel = BlackPixel(display, screen);
	black = BlackPixel(display, screen);
	white = WhitePixel(display, screen);
	depth = DefaultDepth(display, screen);
	window = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, DOOMGENERIC_RESX, DOOMGENERIC_RESY, 0, black, white);

	/* select input */
	XSelectInput(display, window, StructureNotifyMask | KeyPressMask | KeyReleaseMask);

	/* map window */
	XMapWindow(display, window);

	/* configure */
	gc = XCreateGC(display, window, 0, NULL);
	XSetForeground(display, gc, white);
	XkbSetDetectableAutoRepeat(display, 1, 0);

	/* wait for the map notify event */
	for (;;) {
		XNextEvent(display, &e);
		if (e.type == MapNotify)
			break;
	}

	/* create frame */
	image = XCreateImage(display, DefaultVisual(display, screen), depth, ZPixmap, 0, (char *)DG_ScreenBuffer, DOOMGENERIC_RESX, DOOMGENERIC_RESX, 32, 0);
}

/*
 * Draw a frame.
 */
void DG_DrawFrame()
{
	KeySym sym;
	XEvent e;

	if (!display)
		return;

	/* handle key events */
	while (XPending(display) > 0) {
		/* get next event */
		XNextEvent(display, &e);

		if (e.type == KeyPress)
			add_key_to_queue(1, XKeycodeToKeysym(display, e.xkey.keycode, 0));
		else if (e.type == KeyRelease)
			add_key_to_queue(0, XKeycodeToKeysym(display, e.xkey.keycode, 0));
	}

	/* update frame */
	XPutImage(display, window, gc, image, 0, 0, 0, 0, DOOMGENERIC_RESX, DOOMGENERIC_RESY);
}

/*
 * Sleep in millisecond.
 */
void DG_SleepMs(uint32_t ms)
{
	usleep(ms * 1000);
}

/*
 * Get tick in millisecond.
 */
uint32_t DG_GetTicksMs()
{
	struct timezone tzp;
	struct timeval tp;

	gettimeofday(&tp, &tzp);

	return (tp.tv_sec * 1000) + (tp.tv_usec / 1000);
}

/*
 * Get key.
 */
int DG_GetKey(int *pressed, unsigned char *doom_key)
{
	unsigned short key_data;

	/* key queue is empty */
	if (key_queue_ridx == key_queue_widx)
		return 0;

	/* get next key */
	key_data = key_queue[key_queue_ridx++];
	key_queue_ridx %= KEY_QUEUE_SIZE;

	/* decode key */
	*pressed = key_data >> 8;
	*doom_key = key_data & 0xFF;

	return 1;
}

/*
 * Set window title.
 */
void DG_SetWindowTitle(const char *title)
{
	if (window)
		XChangeProperty(display, window, XA_WM_NAME, XA_STRING, 8, PropModeReplace, (const unsigned char *) title, strlen(title));
}

/*
 * Entry point.
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
