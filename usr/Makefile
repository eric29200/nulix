CC		= i386-elf-gcc
LD		= i386-elf-ld
HEADER_PATH	= ../libc/include
C_SOURCES 	= $(wildcard */*.c)
CFLAGS		= -m32 -nostdlib -nostdinc -nodefaultlibs -fno-builtin -Wall -Wextra -I$(HEADER_PATH)
LDFLAGS		= -T link.ld
LDLIBS		= ../libc/libc.a

all: init sh echo

%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

init: init.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

sh: sh.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

echo: echo.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f *.o init sh echo
