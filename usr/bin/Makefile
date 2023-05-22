CC		= i686-linux-gnu-gcc
AR		= i686-linux-gnu-ar
LIBC		= ../../libc/libc.a
CFLAGS		= -O0 -Wall -Wextra -nostdinc -fno-builtin -fno-stack-protector -fno-pic -I../../libc/include -ffreestanding -nostdlib -m32
LDFLAGS		= $(LIBC) -lgcc -static
BUILD_DIR	= ../build/bin
LIBUTILS 	= ../libutils/libutils.a

PROGS		= ls printenv yes logname whoami echo true false clear basename dirname touch cat chmod chown chgrp cmp mkdir mknod	\
			mkfifo ln rm rmdir cp mv uname grep wc head tail hostname time du sleep link unlink pwd which tee date	\
			sort uniq kill loadkeys

all: $(PROGS)

$(PROGS): %: %.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/$@ $< $(LIBUTILS) $(LDFLAGS)

clean:
	rm -f $(BUILD_DIR)/*