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
	cp $(KERNEL) iso/boot/
	grub-mkrescue -o $(ISO) iso
	./scripts/create_rootfs.csh
	sudo $(QEMU) -m $(MEM_SIZE) -serial stdio 			\
		-cdrom $(ISO) 						\
		-drive format=raw,file=$(DISK)				\
		-netdev user,id=mynet0 -device rtl8139,netdev=mynet0

clean:
	make clean -C kernel
	make clean -C usr
	rm -f $(DISK) $(ISO)
