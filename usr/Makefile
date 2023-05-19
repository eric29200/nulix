CC		= i686-linux-gnu-gcc
AR		= i686-linux-gnu-ar
LIBC		= ../libc/libc.a
CFLAGS		= -O0 -Wall -Wextra -nostdinc -fno-builtin -fno-stack-protector -fno-pic -I../libc/include -ffreestanding -nostdlib -m32
LDFLAGS		= ../libc/libc.a -lgcc -static

LIBUTILS 	= libutils/libutils.a
LIBUTILS_SRC	= $(wildcard libutils/*.c)
LIBUTILS_OBJ	= $(LIBUTILS_SRC:.c=.o)

PROGS		= ls printenv yes logname whoami echo true false clear basename dirname touch cat chmod chown chgrp cmp mkdir mknod mkfifo ln rm rmdir cp mv uname sh grep wc head tail hostname time du sleep link unlink pwd which tee date sort

all: $(LIBUTILS) $(PROGS)

$(LIBUTILS): $(LIBUTILS_OBJ)
	$(AR) rc $@ $?

$(PROGS): %: %.c
	@mkdir -p build
	$(CC) $(CFLAGS) -o build/$@ $< $(LIBUTILS) $(LDFLAGS)

clean:
	rm -f build/* $(LIBUTILS) $(LIBUTILS_OBJ)
