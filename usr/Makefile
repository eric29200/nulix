CC		= i686-linux-gnu-gcc
LIBC		= ../libc/libc.a
CFLAGS		= -O0 -Wall -nostdinc -fno-builtin -fno-stack-protector -fno-pic -I../libc/include -ffreestanding -nostdlib -m32
LDFLAGS		= ../libc/libc.a -static

PROGS		= cat

all: $(PROGS)

$(PROGS): %: %.c
	@mkdir -p build
	$(CC) $(CFLAGS) -o build/$@ $< $(LDFLAGS)

clean:
	rm -f build/*
