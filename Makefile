AS		= nasm
CC		= i386-elf-gcc
LD		= i386-elf-ld
KERNEL		= kernel.bin
ISO		= iso/
HEADER_PATH	= include
C_SOURCES 	= $(wildcard lib/*.c drivers/*.c kernel/*.c)
AS_SOURCES 	= $(wildcard boot/*.s)
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
	# move kernel to iso
	mv $(KERNEL) $(ISO)

	# create image
	genisoimage -R -b boot/grub/stage2 -no-emul-boot -boot-load-size 4 -A os -input-charset utf8 -quiet -boot-info-table -o kos.iso $(ISO)
	$(QEMU) -m 32M -cdrom kos.iso

clean:
	rm -f *.o */*.o $(KERNEL) kos.iso
