KERNEL		= kernel/kernel.bin
ISO		= nulix.iso
NJOBS		= 8
MEM_SIZE	= 512M
DISK		= hdd.img
QEMU		= qemu-system-i386

all: run

run:
	make -j$(NJOBS) -C kernel
	make -j$(NJOBS) -C usr
	cp $(KERNEL) iso/boot/
	grub-mkrescue -o $(ISO) iso
	./scripts/create_rootfs.csh
	sudo $(QEMU) -m $(MEM_SIZE) -serial stdio 				\
		-cdrom $(ISO) 							\
    --enable-kvm \
		-drive format=raw,file=$(DISK)					\
		-netdev tap,id=nulix_net					\
		-device rtl8139,netdev=nulix_net,id=nulix_nic			\
		-object filter-dump,id=f1,netdev=nulix_net,file=./traffic.pcap

clean:
	make clean -C kernel
	make clean -C usr
	rm -f $(DISK) $(ISO)
