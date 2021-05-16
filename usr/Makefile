CC		= ../musl/musl-1.2.0-build/bin/musl-gcc
CFLAGS		= -m32 -ffreestanding -fno-stack-protector -Wall -Wextra

all: init

init: init.o
	$(CC) $(CFLAGS) -o $@ $< -static

clean:
	rm -f *.o init
