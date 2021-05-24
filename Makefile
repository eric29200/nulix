KERNEL		= kernel/kernel.bin
ISO		= nulix.iso
NJOBS		= 8
MEM_SIZE	= 256M
DISK		= hdd.img
QEMU		= qemu-system-i386

all: run

run:
	make -j$(NJOBS) -C kernel
	make -j$(NJOBS) -C usr
	grub-mkrescue -o $(ISO) iso
	./scripts/create_rootfs.csh
	$(QEMU) -m $(MEM_SIZE) -serial stdio -cdrom $(ISO) -drive format=raw,file=$(DISK)

clean:
	make clean -C kernel
	make clean -C usr
	rm -f $(DISK) $(ISO)
