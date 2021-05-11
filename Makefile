KERNEL		= kernel/kernel.bin
NJOBS		= 8
MEM_SIZE	= 256M
DISK		= hdd.img
QEMU		= qemu-system-i386

all: run

run:
	make -j$(NJOBS) -C kernel
	./scripts/build_musl.csh
	make -j$(NJOBS) -C usr
	./scripts/create_rootfs.csh
	$(QEMU) -m $(MEM_SIZE) -serial stdio -kernel $(KERNEL) -drive format=raw,file=$(DISK)

clean:
	make clean -C kernel
	make clean -C usr
	rm -f $(DISK) $(ISO)
