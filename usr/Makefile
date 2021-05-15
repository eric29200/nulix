CC		= ../musl/musl-1.2.0-build/bin/musl-gcc
CFLAGS		= -Wall -ffreestanding

all: init sh echo ls cat mkdir touch rm rmdir test_signal pwd

init: init.o
	$(CC) $(CFLAGS) -o $@ $< -static

sh: sh.o
	$(CC) $(CFLAGS) -o $@ $< -static

echo: echo.o
	$(CC) $(CFLAGS) -o $@ $< -static

ls: ls.o
	$(CC) $(CFLAGS) -o $@ $< -static

cat: cat.o
	$(CC) $(CFLAGS) -o $@ $< -static

mkdir: mkdir.o
	$(CC) $(CFLAGS) -o $@ $< -static

touch: touch.o
	$(CC) $(CFLAGS) -o $@ $< -static

rm: rm.o
	$(CC) $(CFLAGS) -o $@ $< -static

rmdir: rmdir.o
	$(CC) $(CFLAGS) -o $@ $< -static

test_signal: test_signal.o
	$(CC) $(CFLAGS) -o $@ $< -static

pwd: pwd.o
	$(CC) $(CFLAGS) -o $@ $< -static

clean:
	rm -f *.o init sh echo ls cat mkdir touch rm rmdir test_signal pwd
