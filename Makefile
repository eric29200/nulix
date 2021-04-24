KERNEL		= kernel/kernel.bin
MEM_SIZE	= 256M
DISK		= hdd.img
DISK_SIZE	= 32M
QEMU		= qemu-system-i386

all: run

$(DISK):
	dd if=/dev/zero of=$(DISK) bs=1 count=1 seek=$(DISK_SIZE)
	mkfs.minix -1 $(DISK)

run: $(DISK)
	make -C kernel
	make -C usr
	sudo mount $(DISK) tmp
	sudo cp usr/init tmp/
	sudo umount tmp
	$(QEMU) -m $(MEM_SIZE) -kernel $(KERNEL) -serial stdio -drive format=raw,file=$(DISK)

clean:
	make clean -C kernel
	make clean -C usr
	rm -f $(DISK)
