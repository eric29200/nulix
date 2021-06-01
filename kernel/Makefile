AS		= nasm
CC		= i686-linux-gnu-gcc
LD		= i686-linux-gnu-ld
OBJCOPY		= i686-linux-gnu-objcopy
KERNEL		= kernel.bin
HEADER_PATH	= include
C_SOURCES 	= $(wildcard */*.c */*/*.c)
AS_SOURCES 	= $(wildcard */*.s)
PSF_SOURCES 	= $(wildcard */*.psf)
OBJS 		= ${C_SOURCES:.c=.o} ${AS_SOURCES:.s=.o} ${PSF_SOURCES:.psf=.o}
CFLAGS		= -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -I$(HEADER_PATH) -D__KERNEL__
LDFLAGS		= -T link.ld
ASFLAGS		= -felf32

%.o: %.psf
	$(OBJCOPY) -O elf32-i386 -B i386 -I binary $< $@

%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $<

$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

clean:
	rm -f *.o */*.o */*/*.o $(KERNEL)
