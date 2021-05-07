AS		= nasm
CC		= i686-linux-gnu-gcc
LD		= i686-linux-gnu-ld
KERNEL		= kernel.bin
HEADER_PATH	= include
C_SOURCES 	= $(wildcard */*.c)
AS_SOURCES 	= $(wildcard */*.s)
OBJS 		= ${C_SOURCES:.c=.o} ${AS_SOURCES:.s=.o}
CFLAGS		= -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -I$(HEADER_PATH) -D__KERNEL__
LDFLAGS		= -T link.ld
ASFLAGS		= -felf32

%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $<

$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

clean:
	rm -f *.o */*.o $(KERNEL)
