KERNEL		= kernel/kernel.bin
ISO		= nulix.iso
NJOBS		= $(shell nproc)
MEM_SIZE	= 3G
DISK1		= hda.img
DISK2		= hdb.img
QEMU		= kvm
args		= `arg="$(filter-out $@,$(MAKECMDGOALS))" && echo $${arg:-${1}}`

all: run

run:
	make -j$(NJOBS) -C kernel
	cp $(KERNEL) iso/boot/
	grub-mkrescue -o $(ISO) iso
	sudo $(QEMU)								\
		-m $(MEM_SIZE)							\
		-serial stdio 							\
		-boot order=d 							\
		-cdrom $(ISO) 							\
		-drive file=$(DISK1),if=none,format=raw,id=disk1		\
		-device ide-hd,drive=disk1,bus=ide.0,unit=0			\
		-drive file=$(DISK2),if=none,format=raw,id=disk2		\
		-device ide-hd,drive=disk2,bus=ide.0,unit=1			\
		-netdev tap,id=nulix_net					\
		-device rtl8139,netdev=nulix_net,id=nulix_nic			\
		-object filter-dump,id=f1,netdev=nulix_net,file=./traffic.pcap	\
		-object rng-random,filename=/dev/urandom,id=rng0		\
		-device virtio-rng-pci,rng=rng0,vectors=2

%:
	@:

port:
	./ports/install.sh $(call args)
	./scripts/create_rootfs.sh

clean:
	make clean -C kernel
	rm -f $(ISO)
