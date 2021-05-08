KERNEL		= kernel/kernel.bin
NJOBS		= 8
MEM_SIZE	= 256M
DISK		= hdd.img
QEMU		= qemu-system-i386

all: run

run:
	make -j$(NJOBS) -C kernel
	make -j$(NJOBS) -C libc
	make -j$(NJOBS) -C usr
	./create_rootfs.csh
	$(QEMU) -m $(MEM_SIZE) -kernel $(KERNEL) -serial stdio -drive format=raw,file=$(DISK)

clean:
	make clean -C kernel
	make clean -C libc
	make clean -C usr
	rm -f $(DISK)
