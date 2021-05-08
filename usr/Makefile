CC		= i686-linux-gnu-gcc
LD		= i686-linux-gnu-ld
HEADER_PATH	= ../musl/musl-1.2.2-build/include
CFLAGS		= -I$(HEADER_PATH)
LDLIBS		= ../musl/musl-1.2.2-build/lib/libc.a

all: init sh echo ls cat mkdir touch rm rmdir

init: init.o
	$(CC) $(CFLAGS) -o $@ $< -static $(LDLIBS)

sh: sh.o
	$(CC) $(CFLAGS) -o $@ $< -static $(LDLIBS)

echo: echo.o
	$(CC) $(CFLAGS) -o $@ $< -static $(LDLIBS)

ls: ls.o
	$(CC) $(CFLAGS) -o $@ $< -static $(LDLIBS)

cat: cat.o
	$(CC) $(CFLAGS) -o $@ $< -static $(LDLIBS)

mkdir: mkdir.o
	$(CC) $(CFLAGS) -o $@ $< -static $(LDLIBS)

touch: touch.o
	$(CC) $(CFLAGS) -o $@ $< -static $(LDLIBS)

rm: rm.o
	$(CC) $(CFLAGS) -o $@ $< -static $(LDLIBS)

rmdir: rmdir.o
	$(CC) $(CFLAGS) -o $@ $< -static $(LDLIBS)

clean:
	rm -f *.o init sh echo ls cat mkdir touch rm rmdir
