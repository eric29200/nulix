CC		= ../musl/musl-1.2.2-build/bin/musl-gcc
CFLAGS		= -m32 -ffreestanding -fno-stack-protector -Wall -Wextra

all: init wallpaper

init: init.o
	$(CC) $(CFLAGS) -o $@ $< -static

wallpaper: wallpaper.o
	$(CC) $(CFLAGS) -o $@ $< -static

clean:
	rm -f *.o init wallpaper
