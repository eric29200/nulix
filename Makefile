KERNEL		= kernel/kernel.bin
ISO		= nulix.iso
NJOBS		= $(shell nproc)
MEM_SIZE	= 12M
DISK1		= hda.img
QEMU		= kvm
BOCHS_CONF	= bochs.conf
args		= `arg="$(filter-out $@,$(MAKECMDGOALS))" && echo $${arg:-${1}}`

all: run

run:
	make -j$(NJOBS) -C kernel
	cp $(KERNEL) iso/boot/
	grub-mkrescue -o $(ISO) iso
	sudo $(QEMU)										\
		-m $(MEM_SIZE)									\
		-serial stdio 									\
		-boot order=d 									\
		-drive file=$(DISK1),if=none,format=raw,id=disk1				\
		-device ide-hd,drive=disk1,bus=ide.0,unit=0					\
		-drive file=$(ISO),if=none,format=raw,id=cdrom1					\
		-device ide-cd,drive=cdrom1,bus=ide.1,unit=0					\
		-netdev tap,id=nulix_net							\
		-device rtl8139,netdev=nulix_net,id=nulix_nic					\
		-object filter-dump,id=f1,netdev=nulix_net,file=./traffic.pcap			\
		-object rng-random,filename=/dev/urandom,id=rng0				\
		-device virtio-rng-pci,rng=rng0,vectors=2					\
		-fsdev local,id=hostshare,path=/home/eric/tmp,security_model=passthrough	\
		-device virtio-9p-pci,fsdev=hostshare,mount_tag=hostshare

bochs:
	make -j$(NJOBS) -C kernel
	cp $(KERNEL) iso/boot/
	grub-mkrescue -o $(ISO) iso
	bochs -q -f $(BOCHS_CONF)

%:
	@:

port:
	./ports/install.sh $(call args)
	./scripts/create_rootfs.sh

clean:
	make clean -C kernel
	rm -f $(ISO)
