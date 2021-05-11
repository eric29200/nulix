KERNEL		= kernel/kernel.bin
NJOBS		= 8
MEM_SIZE	= 256M
ISO		= nulix.iso
DISK		= hdd.img
QEMU		= qemu-system-i386

all: run

run:
	make -j$(NJOBS) -C kernel
	cp $(KERNEL) iso/
	#./scripts/build_musl.csh
	make -j$(NJOBS) -C usr
	./scripts/create_iso.csh
	./scripts/create_rootfs.csh
	$(QEMU) -m $(MEM_SIZE) -serial stdio -cdrom $(ISO) -drive format=raw,file=$(DISK)

clean:
	make clean -C kernel
	make clean -C usr
	rm -f $(DISK) $(ISO)
