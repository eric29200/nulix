AS		= nasm
CC		= i386-elf-gcc
LD		= i386-elf-ld
KERNEL		= kernel.bin
HEADER_PATH	= include
C_SOURCES 	= $(wildcard */*.c)
AS_SOURCES 	= $(wildcard */*.s)
OBJS 		= ${C_SOURCES:.c=.o} ${AS_SOURCES:.s=.o}
CFLAGS		= -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -I$(HEADER_PATH) -std=c11 -pedantic -D__KERNEL__
LDFLAGS		= -Tlink.ld
ASFLAGS		= -felf32
QEMU		= qemu-system-i386

all: run

%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $<

$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

run: $(KERNEL)
	$(QEMU) -m 32M -kernel $(KERNEL)

clean:
	rm -f *.o */*.o $(KERNEL)
