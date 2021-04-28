CC		= i386-elf-gcc
LD		= i386-elf-ld
HEADER_PATH	= ../libc
C_SOURCES 	= $(wildcard */*.c)
CFLAGS		= -m32 -nostdlib -Wall -Wextra -I$(HEADER_PATH)
LDFLAGS		= -Tlink.ld

all: init sh

%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

init: init.o
	$(LD) $(LDFLAGS) -o $@ $^

sh: sh.o
	$(LD) $(LDFLAGS) -o $@ $^

clean:
	rm -f *.o init sh
