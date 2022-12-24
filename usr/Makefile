CC		= ../musl/musl-install/bin/i386-linux-musl-gcc
CFLAGS		= -m32 -ffreestanding -fno-stack-protector -Wall -Wextra

all: mmap_test

mmap_test: mmap_test.o
	$(CC) $(CFLAGS) -o $@ $< -static

clean:
	rm -f *.o mmap_test