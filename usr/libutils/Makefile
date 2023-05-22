CC		= i686-linux-gnu-gcc
AR		= i686-linux-gnu-ar
LIBC		= ../libc/libc.a
CFLAGS		= -O0 -Wall -Wextra -nostdinc -fno-builtin -fno-stack-protector -fno-pic -I../../libc/include -ffreestanding -nostdlib -m32

LIBUTILS 	= libutils.a
LIBUTILS_SRC	= $(wildcard *.c)
LIBUTILS_OBJ	= $(LIBUTILS_SRC:.c=.o)

all: $(LIBUTILS)

$(LIBUTILS): $(LIBUTILS_OBJ)
	$(AR) rc $@ $?

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(LIBUTILS) $(LIBUTILS_OBJ)
