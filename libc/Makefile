CC		= i686-linux-gnu-gcc
AR		= i686-linux-gnu-ar
LIBC		= libc.a
C_SOURCES 	= $(wildcard *.c */*.c */*/*.c)
AS_SOURCES 	= $(wildcard *.s */*.s */*/*.s)
OBJS 		= ${C_SOURCES:.c=.o} ${AS_SOURCES:.s=.o}
CFLAGS		= -O0 -Wall -Wextra -nostdinc -fno-builtin -fno-stack-protector -fno-pic -nostdlib -nodefaultlibs -m32 -Iinclude/
ASFLAGS		= -Wall -nostdinc -fno-builtin -nostdlib -nodefaultlibs -m32

all: $(LIBC)

$(LIBC): $(OBJS)
	$(AR) rcs $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(ASFLAGS) -c $< -o $@

clean:
	rm -f *.o */*.o */*/*.o $(LIBC)
