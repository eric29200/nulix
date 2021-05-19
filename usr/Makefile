CC		= ../musl/musl-1.2.2-build/bin/musl-gcc
CFLAGS		= -m32 -ffreestanding -fno-stack-protector -Wall -Wextra

all: init echo ls cat mkdir touch rm rmdir pwd

init: init.o
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

pwd: pwd.o
	$(CC) $(CFLAGS) -o $@ $< -static

clean:
	rm -f *.o init echo ls cat mkdir touch rm rmdir pwd
