KERNEL		= kernel/kernel.bin
ISO		= nulix.iso
NJOBS		= $(shell nproc)
MEM_SIZE	= 64M
DISK		= hda.img
QEMU		= kvm
args		= `arg="$(filter-out $@,$(MAKECMDGOALS))" && echo $${arg:-${1}}`

all: run

run:
	make -j$(NJOBS) -C kernel
	cp $(KERNEL) iso/boot/
	grub-mkrescue -o $(ISO) iso
	$(QEMU)									\
		-m $(MEM_SIZE)							\
		-serial stdio 							\
		-boot order=d 							\
		-cdrom $(ISO) 							\
		-drive format=raw,file=$(DISK)					\
		-netdev user,id=nulix_net					\
		-device rtl8139,netdev=nulix_net,id=nulix_nic			\
		-object filter-dump,id=f1,netdev=nulix_net,file=./traffic.pcap

%:
	@:

port:
	./ports/install.sh $(call args)
	./scripts/create_rootfs.sh

clean:
	make clean -C kernel
	rm -f $(ISO)
