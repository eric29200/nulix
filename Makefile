AS		= nasm
CC		= i386-elf-gcc
LD		= i386-elf-ld
C_SOURCES 	= $(wildcard *.c */*.c)
AS_SOURCES 	= $(wildcard *.s */*.s)
OBJS 		= ${C_SOURCES:.c=.o} ${AS_SOURCES:.s=.o}
CFLAGS		= -nostdlib -nostdinc -fno-builtin -fno-stack-protector
LDFLAGS		= -Tboot/link.ld
ASFLAGS		= -felf
QEMU		= qemu-system-i386

all: run

%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $<

kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

run: kernel.bin
	sudo losetup /dev/loop0 grub/floppy.img
	sudo mount /dev/loop0 grub
	sudo cp kernel.bin grub/kernel
	sudo umount grub
	sudo losetup -d /dev/loop0
	$(QEMU) -fda grub/floppy.img

clean:
	rm -f *.o */*.o kernel.bin
