CC		= i386-elf-gcc
LD		= i386-elf-ld
AR		= i386-elf-ar
LIBC		= libc.a
HEADER_PATH	= include
C_SOURCES 	= $(wildcard */*.c)
OBJS		= ${C_SOURCES:.c=.o}
CFLAGS		= -m32 -nostdlib -nostdinc -nodefaultlibs -fno-builtin -Wall -Wextra -I$(HEADER_PATH)
ARFLAGS		= rcs

%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

$(LIBC): $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

clean:
	rm -f *.o */*.o $(LIBC)
