CC		= ../musl/musl-install/bin/i386-linux-musl-gcc
CFLAGS		= -m32 -ffreestanding -fno-stack-protector -Wall -Wextra

all: init test_clone

init: init.o
	$(CC) $(CFLAGS) -o $@ $< -static

test_clone: test_clone.o
	$(CC) $(CFLAGS) -o $@ $< -static

clean:
	rm -f *.o init test_clone
