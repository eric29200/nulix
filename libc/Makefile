CC		= i686-linux-gnu-gcc
AR		= i686-linux-gnu-ar
LIBC		= libc.a
C_SOURCES 	= $(wildcard *.c */*.c */*/*.c)
OBJS 		= ${C_SOURCES:.c=.o}
CFLAGS		= -O0 -Wall -nostdinc -fno-builtin -fno-stack-protector -fno-pic -nostdlib -nodefaultlibs -m32 -Iinclude/
ASFLAGS		= -Wall -nostdinc -fno-builtin -nostdlib -nodefaultlibs -m32

all: $(LIBC)

$(LIBC): $(OBJS)
	$(AR) rcs $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o */*.o */*/*.o $(LIBC)
