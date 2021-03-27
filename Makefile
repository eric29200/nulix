AS		= nasm
CC		= i386-elf-gcc
LD		= i386-elf-ld
KERNEL		= kernel.bin
ISO		= iso/
C_SOURCES 	= $(wildcard *.c */*.c)
AS_SOURCES 	= $(wildcard *.s */*.s)
OBJS 		= ${C_SOURCES:.c=.o} ${AS_SOURCES:.s=.o}
CFLAGS		= -nostdlib -nostdinc -fno-builtin -fno-stack-protector -Wall -Wextra
LDFLAGS		= -Tlink.ld
ASFLAGS		= -felf
QEMU		= qemu-system-i386

all: run

%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $<

$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

run: $(KERNEL)
	cp $(KERNEL) $(ISO)
	genisoimage -R -b boot/grub/stage2 -no-emul-boot -boot-load-size 4 -A os -quiet -boot-info-table -o kos.iso $(ISO)
	$(QEMU) -m 32M -cdrom kos.iso

clean:
	rm -f *.o */*.o $(KERNEL) kos.iso
