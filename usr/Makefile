CC		= i386-elf-gcc
LD		= i386-elf-ld
C_SOURCES 	= $(wildcard */*.c)
CFLAGS		= -m32 -nostdlib -Wall -Wextra

all: init

%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

init: init.o
	$(LD) -o $@ $^

clean:
	rm -f *.o init
