CC		= i686-linux-gnu-gcc
AR		= i686-linux-gnu-ar
LIBC		= ../../libc/libc.a
CFLAGS		= -O0 -Wall -Wextra -nostdinc -fno-builtin -fno-stack-protector -fno-pic -I../../libc/include -ffreestanding -nostdlib -m32
LDFLAGS		= $(LIBC) -lgcc -static
BUILD_DIR	= ../build/sbin
LIBUTILS 	= ../libutils/libutils.a

PROGS		= init

$(PROGS): %: %.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/$@ $< $(LIBUTILS) $(LDFLAGS)

clean:
	rm -f $(BUILD_DIR)/*
