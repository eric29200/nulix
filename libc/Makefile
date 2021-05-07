CC		= i686-linux-gnu-gcc
LD		= i686-linux-gnu-ld
AR		= i686-linux-gnu-ar
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
