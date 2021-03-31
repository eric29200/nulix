CC		= gcc
CFLAGS		= -Wall -Wextra -pedantic

all: mkfs.kfs

%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

mkfs.kfs:
	${CC} ${CFLAGS_USR} -o mkfs.kfs mkfs.kfs.c

clean:
	rm -f *.o mkfs.kfs
