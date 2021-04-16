CC		= i386-elf-gcc
LD		= i386-elf-ld
C_SOURCES 	= $(wildcard */*.c)
CFLAGS		= -m32 -nostdlib -Wall -Wextra
LDFLAGS		= -Tlink.ld

all: init

%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

init: init.o
	$(LD) $(LDFLAGS) -o $@ $^

clean:
	rm -f *.o init
