AS		= nasm
CC		= i686-linux-gnu-gcc
LD		= i686-linux-gnu-ld
OBJCOPY		= i686-linux-gnu-objcopy
KERNEL		= kernel.bin
HEADER_PATH	= include
C_SOURCES 	= $(wildcard */*.c */*/*.c)
AS_SOURCES 	= $(wildcard */*.S)
PSF_SOURCES 	= $(wildcard */*.psf)
OBJS 		= ${C_SOURCES:.c=.o} ${AS_SOURCES:.S=.o} ${PSF_SOURCES:.psf=.o}
CFLAGS		= -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -I$(HEADER_PATH) -D__KERNEL__ -g
LDFLAGS		= -T link.ld
ASFLAGS		= -felf32
LIBGCC		= $(shell $(CC) -print-libgcc-file-name)

%.o: %.psf
	$(OBJCOPY) -O elf32-i386 -B i386 -I binary $< $@

%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

%.o: %.S
	${CC} ${CFLAGS} -c $< -o $@

$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBGCC)

clean:
	rm -f *.o */*.o */*/*.o $(KERNEL)
